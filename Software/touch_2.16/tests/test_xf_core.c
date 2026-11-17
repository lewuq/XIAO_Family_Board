/**
 * @file test_xf_core.c
 * @brief Host-side tests of the reducer, the selection set and the timer rules.
 *
 * xf_core and xf_data are pure C: no LVGL, no driver header, no clock and no
 * allocation. That is exactly why they were separated from the UI (code rules
 * 4.1/4.2), so every acceptance rule that can be decided without a panel is
 * decided here instead of on the bench.
 *
 * Build and run on a host (no ESP-IDF needed):
 *
 *     gcc -std=gnu17 -Wall -Wextra -I ../components/xf_core/include \
 *         -I ../components/xf_data/include -I ../components/xf_data/generated \
 *         test_xf_core.c ../components/xf_core/xf_compare.c \
 *         ../components/xf_core/xf_idle.c ../components/xf_core/xf_state_reducer.c \
 *         ../components/xf_data/xf_product_lookup.c \
 *         ../components/xf_data/generated/xf_product_catalog.c -o test_xf_core
 *     ./test_xf_core
 *
 * The exit code is the number of failed checks, so a CI job can use it directly.
 */

#include <stdio.h>
#include <string.h>

#include "xf_app_state.h"
#include "xf_config.h"
#include "xf_content_status.h"
#include "xf_event.h"
#include "xf_product.h"
#include "xf_touch_protocol.h"
#include "xf_strings.h"
#include "xf_feedback_tone.h"

static int s_failures = 0;
static int s_checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        s_checks++;                                                                    \
        if (!(cond)) {                                                                 \
            s_failures++;                                                              \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                     \
        }                                                                              \
    } while (0)

#define CHECK_EQ(actual, expected)                                                     \
    do {                                                                               \
        long long a_ = (long long)(actual);                                            \
        long long e_ = (long long)(expected);                                          \
        s_checks++;                                                                    \
        if (a_ != e_) {                                                                \
            s_failures++;                                                              \
            printf("FAIL %s:%d  %s == %s (got %lld, want %lld)\n", __FILE__, __LINE__, \
                   #actual, #expected, a_, e_);                                        \
        }                                                                              \
    } while (0)

/** @brief Reduce one event and return its result, keeping the effects private. */
static xf_result_t push(xf_app_state_t *state, xf_event_type_t type, uint64_t now_ms,
                        xf_effects_t *effects)
{
    xf_event_t event = xf_event_make(type, now_ms);
    return xf_app_reduce(state, &event, effects);
}

static xf_result_t push_product(xf_app_state_t *state, xf_event_type_t type, uint8_t product,
                                uint64_t now_ms)
{
    xf_event_t event = xf_event_make(type, now_ms);
    event.product_id = product;
    xf_effects_t effects;
    return xf_app_reduce(state, &event, &effects);
}

static xf_result_t push_index(xf_app_state_t *state, xf_event_type_t type, int16_t index,
                              uint64_t now_ms)
{
    xf_event_t event = xf_event_make(type, now_ms);
    event.index = index;
    xf_effects_t effects;
    return xf_app_reduce(state, &event, &effects);
}

/** @brief A state that already passed the first frame and sits on ATTRACT. */
static void start_attract(xf_app_state_t *state)
{
    xf_effects_t effects;
    xf_app_state_init(state);
    CHECK_EQ(push(state, XF_EVENT_BOOT_READY, 0U, &effects), XF_OK);
}

/* ------------------------------------------------------------------------- */
/* Boot, carousel timing, wrap-around ------------------------------------- */
/* ------------------------------------------------------------------------- */

static void test_boot_and_carousel(void)
{
    xf_app_state_t state;
    xf_effects_t effects;

    xf_app_state_init(&state);
    CHECK_EQ(state.screen, XF_SCREEN_BOOT);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_INVALID);
    CHECK_EQ(state.manufacturer_filter, -1);

    CHECK_EQ(push(&state, XF_EVENT_BOOT_READY, 0U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);
    CHECK_EQ(state.next_slide_deadline_ms, XF_AUTOPLAY_INTERVAL_MS);
    CHECK(effects.screen_changed);
    CHECK(effects.reset_slide_clock);

    /* The deadline is exclusive: 2999 ms is early, 3000 ms is due. */
    CHECK(!xf_autoplay_due(&state, XF_AUTOPLAY_INTERVAL_MS - 1U));
    CHECK(xf_autoplay_due(&state, XF_AUTOPLAY_INTERVAL_MS));

    /* A SLIDE_DUE event before the deadline must be refused, never applied. */
    CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, 1000U, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);

    CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, 3000U, &effects), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21_PLUS);
    CHECK_EQ(state.next_slide_deadline_ms, 6000U);
    CHECK(effects.slide_committed);

    /* Missing three slots commits exactly one slide and re-anchors to now, so
     * the carousel never bursts to catch up (design note 8.7). */
    CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, 21000U, &effects), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_STM32C5);
    CHECK_EQ(state.next_slide_deadline_ms, 21000U + XF_AUTOPLAY_INTERVAL_MS);

    /* Walk the whole catalogue and prove the order wraps at the last product. */
    start_attract(&state);
    uint64_t now = 0U;
    for (uint8_t i = 1U; i <= XF_PRODUCT_COUNT; i++) {
        now += XF_AUTOPLAY_INTERVAL_MS;
        CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, now, &effects), XF_OK);
        CHECK_EQ(state.current_product_id, i == XF_PRODUCT_COUNT ? 0U : i);
    }
}

/* ------------------------------------------------------------------------- */
/* Press semantics: freeze, tap, drag, double press ---------------------- */
/* ------------------------------------------------------------------------- */

static void test_press_pause_and_tap(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);

    /* Half a slot in, the bar reads 500 permille. */
    CHECK_EQ(xf_autoplay_progress_permille(&state, 1500U), 500U);

    CHECK_EQ(push(&state, XF_EVENT_TOUCH_DOWN, 1500U, &effects), XF_OK);
    CHECK(state.pointer_down);
    CHECK_EQ(state.interaction_product_id, XF_PRODUCT_SAMD21);

    /* The bar is frozen, and the deadline cannot fire while the finger is down,
     * even though the time has passed: a press must not change the product. */
    CHECK_EQ(xf_autoplay_progress_permille(&state, 2500U), 500U);
    CHECK(!xf_autoplay_due(&state, 4000U));
    CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, 4000U, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);

    /* A second press without a release is a lost report, not a new gesture. */
    CHECK_EQ(push(&state, XF_EVENT_TOUCH_DOWN, 1600U, &effects), XF_ERR_INVALID_STATE);

    /* A release alone cannot steal the footer button's independent click. */
    CHECK_EQ(push(&state, XF_EVENT_TOUCH_UP, 1700U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK(!effects.screen_changed);
    CHECK(!effects.content_changed);
    xf_event_t click = xf_event_make(XF_EVENT_OPEN_PRODUCT, 1701U);
    click.product_id = state.interaction_product_id;
    CHECK_EQ(xf_app_reduce(&state, &click, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_DETAIL);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);
    CHECK(effects.screen_changed);
    CHECK_EQ(state.last_input_release_ms, 1700U);

    /* Back returns to the carousel and restarts a full 3000 ms window. */
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 2000U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK_EQ(state.next_slide_deadline_ms, 0U);
    CHECK(state.autoplay_paused);
}

static void test_drag_cancels_tap(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);

    CHECK_EQ(push(&state, XF_EVENT_TOUCH_DOWN, 100U, &effects), XF_OK);

    /* One pixel below the slop is still a tap candidate ... */
    xf_event_t move = xf_event_make(XF_EVENT_TOUCH_MOVE, 150U);
    move.value = XF_TOUCH_SLOP_PX;
    CHECK_EQ(xf_app_reduce(&state, &move, &effects), XF_OK);
    CHECK(!state.tap_cancelled);

    /* ... one pixel above it is a drag. */
    move = xf_event_make(XF_EVENT_TOUCH_MOVE, 160U);
    move.value = XF_TOUCH_SLOP_PX + 1;
    CHECK_EQ(xf_app_reduce(&state, &move, &effects), XF_OK);
    CHECK(state.tap_cancelled);

    CHECK_EQ(push(&state, XF_EVENT_TOUCH_UP, 200U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK(!state.pointer_down);
    /* A cancelled tap restarts the whole window instead of advancing early. */
    CHECK_EQ(state.next_slide_deadline_ms, 0U);
    CHECK(state.autoplay_paused);

    /* A move without a press is not a gesture. */
    move = xf_event_make(XF_EVENT_TOUCH_MOVE, 300U);
    move.value = 40;
    CHECK_EQ(xf_app_reduce(&state, &move, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(push(&state, XF_EVENT_TOUCH_UP, 310U, &effects), XF_ERR_INVALID_STATE);
}

/* ------------------------------------------------------------------------- */
/* Catalog -> detail -> catalog keeps filter, selection and scroll -------- */
/* ------------------------------------------------------------------------- */

static void test_return_context(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);

    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 1000U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);

    CHECK_EQ(push_index(&state, XF_EVENT_SET_FILTER, 3, 1100U), XF_OK);
    CHECK_EQ(state.manufacturer_filter, 3);

    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_S3_PLUS, 1200U),
             XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_NRF54LM20B_SENSE, 1300U),
             XF_OK);

    xf_event_t scroll = xf_event_make(XF_EVENT_SCROLL_STORE, 1400U);
    scroll.value = 120;
    CHECK_EQ(xf_app_reduce(&state, &scroll, &effects), XF_OK);
    CHECK_EQ(state.return_context.scroll_y_px, 120);

    /* Filter is 3 while the product opened here belongs to another vendor: the
     * filter must not be applied to the identity of a tap. */
    CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, XF_PRODUCT_SAMD21, 1500U), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_DETAIL);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);
    CHECK_EQ(state.return_context.screen, XF_SCREEN_CATALOG);

    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 2000U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);
    CHECK_EQ(state.manufacturer_filter, 3);
    CHECK_EQ(state.return_context.scroll_y_px, 120);
    CHECK_EQ(state.selected_count, 2U);
}

/* ------------------------------------------------------------------------- */
/* Selection set: capacity, duplicate, order, gap closing ----------------- */
/* ------------------------------------------------------------------------- */

static void test_selection_set(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 10U, &effects), XF_OK);

    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_RP2350, 20U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C3, 30U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_RA4M1, 40U), XF_OK);
    CHECK_EQ(state.selected_count, XF_COMPARE_CAPACITY);

    /* Toggling an existing member removes it and closes the order gap. */
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C3, 50U), XF_OK);
    CHECK_EQ(state.selected_count, 2U);
    CHECK_EQ(xf_compare_product_at(&state, 0U), XF_PRODUCT_RP2350);
    CHECK_EQ(xf_compare_product_at(&state, 1U), XF_PRODUCT_RA4M1);
    CHECK_EQ(xf_compare_product_at(&state, 2U), XF_PRODUCT_INVALID);

    /* Fill again, then attempt a fourth: the three existing columns must stay. */
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C3, 60U), XF_OK);
    CHECK_EQ(state.selected_count, 3U);

    xf_event_t event = xf_event_make(XF_EVENT_TOGGLE_COMPARE, 70U);
    event.product_id = XF_PRODUCT_MG24_SENSE;
    CHECK_EQ(xf_app_reduce(&state, &event, &effects), XF_ERR_COMPARE_FULL);
    CHECK_EQ(effects.result, XF_ERR_COMPARE_FULL);
    CHECK(effects.content_changed);
    CHECK_EQ(state.notice, XF_NOTICE_SELECTION_MAX);
    CHECK_EQ(state.selected_count, 3U);
    CHECK_EQ(xf_compare_product_at(&state, 0U), XF_PRODUCT_RP2350);
    CHECK_EQ(xf_compare_product_at(&state, 1U), XF_PRODUCT_RA4M1);
    CHECK_EQ(xf_compare_product_at(&state, 2U), XF_PRODUCT_ESP32_C3);
    CHECK(!xf_compare_contains(&state, XF_PRODUCT_MG24_SENSE));

    /* The notice expires on its own after its documented duration. */
    CHECK_EQ(push(&state, XF_EVENT_SCROLL_STORE, 70U + XF_NOTICE_DURATION_MS, &effects), XF_OK);
    CHECK_EQ(state.notice, XF_NOTICE_NONE);

    /* Invalid identifiers are rejected before the set is touched. */
    xf_event_t bad = xf_event_make(XF_EVENT_TOGGLE_COMPARE, 5000U);
    bad.product_id = XF_PRODUCT_INVALID;
    CHECK_EQ(xf_app_reduce(&state, &bad, &effects), XF_ERR_INVALID_ID);

    /* An unknown id never enters the set through the low-level API either. */
    CHECK_EQ(xf_compare_add(&state, XF_PRODUCT_INVALID), XF_ERR_INVALID_ID);
    CHECK_EQ(xf_compare_add(&state, XF_PRODUCT_RP2350), XF_ERR_DUPLICATE);
    CHECK_EQ(xf_compare_remove(&state, XF_PRODUCT_MG24_SENSE), XF_ERR_NOT_PRESENT);

    CHECK_EQ(push(&state, XF_EVENT_CLEAR_SELECTION, 6000U, &effects), XF_OK);
    CHECK_EQ(state.selected_count, 0U);
    CHECK_EQ(state.notice, XF_NOTICE_SELECTION_EMPTY);
}

static void test_compare_gate(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 10U, &effects), XF_OK);

    /* Zero or one product is not a comparison. */
    CHECK(!xf_compare_ready(&state));
    CHECK_EQ(push(&state, XF_EVENT_OPEN_COMPARE, 20U, &effects), XF_ERR_COMPARE_TOO_FEW);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);
    CHECK_EQ(state.notice, XF_NOTICE_SELECTION_MIN);

    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_RP2040, 30U), XF_OK);
    CHECK(!xf_compare_ready(&state));

    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_RP2040_PLUS, 40U), XF_OK);
    CHECK(xf_compare_ready(&state));
    CHECK_EQ(push(&state, XF_EVENT_OPEN_COMPARE, 50U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_COMPARE);

    /* Tabs and the differences switch are pure view state. */
    CHECK_EQ(push_index(&state, XF_EVENT_SET_COMPARE_CATEGORY, XF_COMPARE_CATEGORY_SENSORS, 60U),
             XF_OK);
    CHECK_EQ(state.compare_category, XF_COMPARE_CATEGORY_SENSORS);
    CHECK_EQ(push_index(&state, XF_EVENT_SET_COMPARE_CATEGORY, XF_COMPARE_CATEGORY_COUNT, 61U),
             XF_ERR_INVALID_ARG);
    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_DIFFERENCES, 62U, &effects), XF_OK);
    CHECK(state.compare_differences_only);

    /* COMPARE -> SELECT -> back goes to CATALOG, not to the table. */
    CHECK_EQ(push(&state, XF_EVENT_EDIT_SELECTION, 70U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_SELECT);
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 80U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);

    /* Losing a column while the return screen is COMPARE lands on SELECT, so a
     * one-product table is never drawn (design note 7). */
    CHECK_EQ(push(&state, XF_EVENT_OPEN_COMPARE, 90U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_COMPARE);
    CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, XF_PRODUCT_RP2040, 95U), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_DETAIL);
    CHECK_EQ(state.return_context.screen, XF_SCREEN_COMPARE);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_RP2040, 100U), XF_OK);
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 110U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_SELECT);
}

/* ------------------------------------------------------------------------- */
/* "Differences only" property ------------------------------------------- */
/* ------------------------------------------------------------------------- */

/** @brief Exact equality of two catalogue cells, including their honesty tags. */
static bool spec_equal(const xf_spec_t *a, const xf_spec_t *b)
{
    if (a->status != b->status || a->kind != b->kind || a->boolean != b->boolean) {
        return false;
    }
    if ((a->text == NULL) != (b->text == NULL)) {
        return false;
    }
    if (a->text != NULL && strcmp(a->text, b->text) != 0) {
        return false;
    }
    if ((a->unit == NULL) != (b->unit == NULL)) {
        return false;
    }
    if (a->unit != NULL && strcmp(a->unit, b->unit) != 0) {
        return false;
    }
    return true;
}

static void test_differences_only_property(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 10U, &effects), XF_OK);

    /* Compare the first two products, whose rows are textually equal often. */
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_SAMD21, 20U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_SAMD21_PLUS, 30U), XF_OK);

    for (uint8_t field = 0U; field < XF_SPEC_FIELD_COUNT; field++) {
        const xf_spec_t *a = xf_product_spec(XF_PRODUCT_SAMD21, field);
        const xf_spec_t *b = xf_product_spec(XF_PRODUCT_SAMD21_PLUS, field);
        CHECK(a != NULL);
        CHECK(b != NULL);
        if (a == NULL || b == NULL) {
            continue;
        }
        bool equal = spec_equal(a, b);
        /* A row of two identical verified values is hidden; anything else is
         * shown, because a gap must never be presented as equality. */
        bool expected_hidden = equal && a->status == XF_SPEC_VERIFIED;
        CHECK_EQ(xf_compare_row_is_identical(&state, field), expected_hidden);
    }

    /* Adding a third product can only make a row non-identical, never the
     * reverse: the predicate is monotonic in the number of columns. */
    bool before[XF_SPEC_FIELD_COUNT];
    for (uint8_t field = 0U; field < XF_SPEC_FIELD_COUNT; field++) {
        before[field] = xf_compare_row_is_identical(&state, field);
    }
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_S3_PLUS, 40U), XF_OK);
    for (uint8_t field = 0U; field < XF_SPEC_FIELD_COUNT; field++) {
        if (before[field]) {
            CHECK(xf_compare_row_is_identical(&state, field) ==
                  (before[field] && xf_compare_row_is_identical(&state, field)));
        }
    }

    /* An out-of-range field is never "identical". */
    CHECK(!xf_compare_row_is_identical(&state, XF_SPEC_FIELD_COUNT));
}

/* ------------------------------------------------------------------------- */
/* Idle recovery ---------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

static void test_idle_recovery(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 1000U, &effects), XF_OK);
    CHECK_EQ(push_index(&state, XF_EVENT_SET_FILTER, 2, 1100U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C6, 1200U), XF_OK);

    /* The idle rule needs a release to start from. */
    xf_event_t release = xf_event_make(XF_EVENT_TOUCH_DOWN, 1000U);
    (void)release;
    state.last_input_release_ms = 2000U;

    CHECK(!xf_idle_expired(&state, 2000U + XF_IDLE_TIMEOUT_MS - 1U));
    CHECK(xf_idle_expired(&state, 2000U + XF_IDLE_TIMEOUT_MS));
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 2000U + XF_IDLE_TIMEOUT_MS - 1U, &effects),
             XF_OK); /* the reducer trusts the caller's predicate only for the bound */
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK(effects.clear_visitor);
    CHECK_EQ(state.selected_count, 0U);
    CHECK_EQ(state.manufacturer_filter, -1);
    CHECK_EQ(state.compare_category, XF_COMPARE_CATEGORY_COMPUTE);
    CHECK(!state.compare_differences_only);
    CHECK_EQ(state.notice, XF_NOTICE_NONE);
    CHECK_EQ(state.next_slide_deadline_ms,
             2000U + XF_IDLE_TIMEOUT_MS - 1U + XF_AUTOPLAY_INTERVAL_MS);

    /* ATTRACT has nothing to recover from, and a finger down blocks the reset. */
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 200000U, &effects), XF_ERR_INVALID_STATE);

    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 200100U, &effects), XF_OK);
    CHECK_EQ(push(&state, XF_EVENT_TOUCH_DOWN, 200200U, &effects), XF_OK);
    CHECK(!xf_idle_expired(&state, 400000U));
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 400000U, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);
}

/* ------------------------------------------------------------------------- */
/* Catalogue and asset contract ------------------------------------------- */
/* ------------------------------------------------------------------------- */

static void test_catalogue_contract(void)
{
    /* Every identifier resolves, and identity is the numeric id, not the name. */
    for (uint8_t id = 0U; id < XF_PRODUCT_COUNT; id++) {
        const xf_product_t *product = xf_product_find(id);
        CHECK(product != NULL);
        if (product == NULL) {
            continue;
        }
        CHECK_EQ(product->id, id);
        CHECK(product->name != NULL && product->name[0] != '\0');
        CHECK(product->short_name != NULL && product->short_name[0] != '\0');
        CHECK(product->manufacturer_index < XF_MANUFACTURER_COUNT);
        CHECK(product->asset < XF_ASSET_COUNT);
        CHECK(xf_product_find_by_key(product->id_string) == product);
        CHECK(xf_product_next(product->id) == (uint8_t)((id + 1U) % XF_PRODUCT_COUNT));
        CHECK(xf_product_prev(product->id) ==
              (uint8_t)((id + XF_PRODUCT_COUNT - 1U) % XF_PRODUCT_COUNT));
    }
    CHECK(xf_product_find(XF_PRODUCT_COUNT) == NULL);
    CHECK(xf_product_find_by_key("not_a_product") == NULL);

    /* A verified empty collection renders "None", so it must be tagged as such
     * rather than as an empty text (design note 6.4). */
    for (uint8_t id = 0U; id < XF_PRODUCT_COUNT; id++) {
        for (uint8_t field = 0U; field < XF_SPEC_FIELD_COUNT; field++) {
            const xf_spec_t *spec = xf_product_spec(id, field);
            CHECK(spec != NULL);
            if (spec == NULL) {
                continue;
            }
            CHECK(spec->status <= XF_SPEC_NOT_APPLICABLE);
            CHECK(spec->kind <= XF_SPEC_KIND_NONE);
            if (spec->kind == XF_SPEC_KIND_NONE) {
                CHECK_EQ(spec->status, XF_SPEC_VERIFIED);
            }
            if (spec->status != XF_SPEC_VERIFIED) {
                /* An unverified cell must not carry a value that could be read
                 * as a fact by any renderer. */
                CHECK(spec->text == NULL || spec->text[0] == '\0' ||
                      spec->kind == XF_SPEC_KIND_EMPTY);
            }
        }
    }
    CHECK(xf_product_spec(XF_PRODUCT_COUNT, 0U) == NULL);
    CHECK(xf_product_spec(0U, XF_SPEC_FIELD_COUNT) == NULL);

    /* Every field belongs to exactly one COMPARE tab and one DETAIL section. */
    uint8_t total = 0U;
    for (uint8_t category = 0U; category < XF_COMPARE_CATEGORY_COUNT; category++) {
        uint8_t count = xf_spec_category_field_count(category);
        total = (uint8_t)(total + count);
        for (uint8_t ordinal = 0U; ordinal < count; ordinal++) {
            uint8_t field = 0xFFU;
            CHECK(xf_spec_category_field_at(category, ordinal, &field));
            CHECK(xf_spec_field_in_category(field, category));
            CHECK_EQ(xf_spec_field_compare_category[field], category);
        }
    }
    CHECK_EQ(total, XF_SPEC_FIELD_COUNT);

    for (uint8_t field = 0U; field < XF_SPEC_FIELD_COUNT; field++) {
        CHECK(xf_spec_field_section[field] < XF_SECTION_COUNT);
    }
}

/* ------------------------------------------------------------------------- */
/* Argument handling ------------------------------------------------------ */
/* ------------------------------------------------------------------------- */

static void test_argument_handling(void)
{
    xf_app_state_t state;
    xf_event_t event = xf_event_make(XF_EVENT_BOOT_READY, 0U);
    xf_effects_t effects;

    xf_app_state_init(&state);
    CHECK_EQ(xf_app_reduce(NULL, &event, &effects), XF_ERR_INVALID_ARG);
    CHECK_EQ(xf_app_reduce(&state, NULL, &effects), XF_ERR_INVALID_ARG);
    CHECK_EQ(xf_app_reduce(&state, &event, NULL), XF_ERR_INVALID_ARG);

    event.type = 0xFEU;
    CHECK_EQ(xf_app_reduce(&state, &event, &effects), XF_ERR_INVALID_ARG);

    CHECK_EQ(xf_app_visible_product(NULL), XF_PRODUCT_INVALID);
    CHECK(!xf_compare_contains(NULL, 0U));
    CHECK_EQ(xf_compare_add(NULL, 0U), XF_ERR_INVALID_ARG);
    CHECK_EQ(xf_compare_remove(NULL, 0U), XF_ERR_INVALID_ARG);
    CHECK_EQ(xf_compare_toggle(NULL, 0U), XF_ERR_INVALID_ARG);
    CHECK(!xf_compare_ready(NULL));
    CHECK_EQ(xf_compare_product_at(NULL, 0U), XF_PRODUCT_INVALID);
    CHECK(!xf_compare_row_is_identical(NULL, 0U));
    CHECK(!xf_autoplay_due(NULL, 1000U));
    CHECK_EQ(xf_autoplay_progress_permille(NULL, 1000U), 0U);
    CHECK(!xf_idle_expired(NULL, 1000U));
    xf_app_state_init(NULL); /* must not crash */
    xf_compare_clear(NULL);
}

/* ------------------------------------------------------------------------- */


/* Manual controls must never be overtaken by the carousel or release gesture. */
static void test_manual_navigation(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK(!state.autoplay_paused);

    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, -1, 100U), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_COUNT - 1U);
    CHECK(state.autoplay_paused);
    CHECK_EQ(state.next_slide_deadline_ms, 0U);
    CHECK(!xf_autoplay_due(&state, 9999U));
    CHECK_EQ(xf_autoplay_progress_permille(&state, 9999U), 0U);

    CHECK_EQ(push(&state, XF_EVENT_TOUCH_DOWN, 200U, &effects), XF_OK);
    CHECK_EQ(push(&state, XF_EVENT_TOUCH_UP, 210U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK(state.autoplay_paused);
    CHECK(!effects.content_changed);
    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, 1, 220U), XF_OK);
    CHECK_EQ(state.current_product_id, 0U);
    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, 0, 230U), XF_ERR_INVALID_ARG);

    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_AUTOPLAY, 300U, &effects), XF_OK);
    CHECK(!state.autoplay_paused);
    CHECK_EQ(state.next_slide_deadline_ms, 3300U);
    CHECK(!xf_autoplay_due(&state, 3299U));
    CHECK(xf_autoplay_due(&state, 3300U));
    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_AUTOPLAY, 400U, &effects), XF_OK);
    CHECK(state.autoplay_paused);

    /* A real Browse click reaches the catalogue without an intermediate detail. */
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 500U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);
    CHECK_EQ(state.next_slide_deadline_ms, 0U);
    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, 1, 510U), XF_ERR_INVALID_STATE);
    CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, XF_PRODUCT_RP2040, 600U), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_DETAIL);
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, id, 700U + id), XF_OK);
        CHECK_EQ(state.current_product_id, id);
        CHECK_EQ(state.return_context.screen, XF_SCREEN_CATALOG);
    }
    CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, XF_PRODUCT_INVALID, 800U), XF_ERR_INVALID_ID);
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 900U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_CATALOG);
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 1000U, &effects), XF_OK);
    CHECK_EQ(state.screen, XF_SCREEN_ATTRACT);
    CHECK(state.autoplay_paused);

    /* The exhibition idle rule still restores autoplay after a manual visit. */
    state.last_input_release_ms = 1000U;
    CHECK(!xf_idle_expired(&state, 1000U + XF_IDLE_TIMEOUT_MS - 1U));
    CHECK(xf_idle_expired(&state, 1000U + XF_IDLE_TIMEOUT_MS));
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 1000U + XF_IDLE_TIMEOUT_MS, &effects), XF_OK);
    CHECK(!state.autoplay_paused);
    CHECK_EQ(state.next_slide_deadline_ms, 1000U + XF_IDLE_TIMEOUT_MS + XF_AUTOPLAY_INTERVAL_MS);
    CHECK_EQ(state.current_product_id, xf_product_next(XF_PRODUCT_COUNT - 1U));
}


/** V1 has a K9 hole, little-endian sequences, and a valid sequence zero. */
static void test_touch_protocol(void)
{
    uint32_t seen = 0U;
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        uint8_t key = xf_tp_product_to_key(id);
        CHECK(key != 9U && key <= 18U);
        CHECK_EQ(xf_tp_key_to_product(key), id);
        CHECK_EQ(seen & (UINT32_C(1) << key), 0U);
        seen |= UINT32_C(1) << key;
        CHECK_EQ(xf_tp_keys_to_products(UINT32_C(1) << key), UINT32_C(1) << id);
        CHECK_EQ(xf_tp_products_to_keys(UINT32_C(1) << id), UINT32_C(1) << key);
    }
    CHECK_EQ(seen, XF_TP_VALID_KEY_MASK);
    CHECK_EQ(xf_tp_product_to_key(XF_PRODUCT_ESP32_C5), 0x0AU);
    CHECK_EQ(xf_tp_product_to_key(XF_PRODUCT_ESP32_S3_SENSE), 0x0DU);
    CHECK_EQ(xf_tp_product_to_key(XF_PRODUCT_NRF54LM20A_SENSE), 0x11U);
    CHECK_EQ(xf_tp_key_to_product(9U), XF_PRODUCT_INVALID);
    CHECK_EQ(xf_tp_key_to_product(19U), XF_PRODUCT_INVALID);
    CHECK_EQ(xf_tp_product_to_key(XF_PRODUCT_COUNT), XF_PRODUCT_INVALID);
    CHECK_EQ(xf_tp_keys_to_products(seen), XF_TP_VALID_PRODUCT_MASK);
    uint8_t bitmap[4] = {1U, 0U, 2U, 0U};
    CHECK_EQ(xf_tp_u32(bitmap), (UINT32_C(1) << 17) | 1U);

    xf_tp_status_t status;
    uint8_t raw[4] = {1U, XF_TP_READY | XF_TP_OVERFLOW, 32U, 6U};
    CHECK(xf_tp_decode_status(raw, &status));
    CHECK_EQ(status.count, 32U);
    raw[0] = 2U; CHECK(!xf_tp_decode_status(raw, &status)); raw[0] = 1U;
    raw[1] |= 0x20U; CHECK(!xf_tp_decode_status(raw, &status)); raw[1] &= 0x1FU;
    raw[2] = 33U; CHECK(!xf_tp_decode_status(raw, &status)); raw[2] = 0U;
    raw[3] = 7U; CHECK(!xf_tp_decode_status(raw, &status));
    CHECK(!xf_tp_decode_status(NULL, &status));

    xf_tp_event_t event;
    uint8_t down_zero[4] = {XF_TP_DOWN, 0x12U, 0U, 0U};
    CHECK(xf_tp_decode_event(down_zero, &event));
    CHECK_EQ(event.seq, 0U);
    xf_tp_pending_t pending = {.delivered = true, .event = event};
    CHECK(xf_tp_already_delivered(&pending, &event));
    event.action = XF_TP_UP;
    CHECK(!xf_tp_already_delivered(&pending, &event));
    event = pending.event; event.seq = 1U;
    CHECK(!xf_tp_already_delivered(&pending, &event));
    event = pending.event; event.key = 0U;
    CHECK(!xf_tp_already_delivered(&pending, &event));
    pending.delivered = false; event = pending.event;
    CHECK(!xf_tp_already_delivered(&pending, &event));
    uint8_t empty[4] = {0xFFU, 0xFFU, 0U, 0U};
    CHECK(xf_tp_decode_event(empty, &event));
    empty[2] = 1U; CHECK(!xf_tp_decode_event(empty, &event));
    down_zero[1] = 9U; CHECK(!xf_tp_decode_event(down_zero, &event));
    uint8_t up[4] = {XF_TP_UP, 17U, 0xFFU, 0xFFU};
    CHECK(xf_tp_decode_event(up, &event)); CHECK_EQ(event.seq, 0xFFFFU);
    uint8_t ack[3];
    xf_tp_ack_bytes(0x1234U, ack);
    CHECK_EQ(ack[0], 0x20U); CHECK_EQ(ack[1], 0x34U); CHECK_EQ(ack[2], 0x12U);
    xf_tp_ack_bytes(0U, ack);
    CHECK_EQ(ack[1], 0U); CHECK_EQ(ack[2], 0U);
}

/** Held external keys block autoplay; unchanged polls never postpone idle. */
static void test_external_and_language(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(state.language, 0U);
    CHECK_EQ(xf_app_displayed_products(&state), 1U);
    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_LANGUAGE, 10U, &effects), XF_OK);
    CHECK_EQ(state.language, 1U);
    CHECK(effects.content_changed);
    xf_event_t remote = xf_event_make(XF_EVENT_REMOTE_TOUCH, 100U);
    remote.product_id = XF_PRODUCT_NRF54LM20A_SENSE;
    remote.index = 1;
    remote.value = (int32_t)(UINT32_C(1) << remote.product_id);
    CHECK_EQ(xf_app_reduce(&state, &remote, &effects), XF_OK);
    CHECK_EQ(state.current_product_id, remote.product_id);
    CHECK_EQ(state.screen, XF_SCREEN_DETAIL);
    CHECK_EQ(xf_app_displayed_products(&state), (uint32_t)remote.value);
    CHECK(!xf_autoplay_due(&state, 100000U));
    CHECK(!xf_idle_expired(&state, 100000U));
    xf_event_t snapshot = xf_event_make(XF_EVENT_REMOTE_STATE, 500U);
    snapshot.value = remote.value;
    CHECK_EQ(xf_app_reduce(&state, &snapshot, &effects), XF_OK);
    CHECK_EQ(state.last_input_release_ms, 100U);
    CHECK(!effects.content_changed && !effects.screen_changed);
    remote.now_ms = 1000U; remote.index = 0; remote.value = 0;
    CHECK_EQ(xf_app_reduce(&state, &remote, &effects), XF_OK);
    CHECK_EQ(state.external_touch_mask, 0U);
    CHECK(!xf_idle_expired(&state, 6999U));
    snapshot.now_ms = 6900U; snapshot.value = 0;
    CHECK_EQ(xf_app_reduce(&state, &snapshot, &effects), XF_OK);
    CHECK_EQ(state.last_input_release_ms, 1000U);
    CHECK(xf_idle_expired(&state, 7000U));
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 7000U, &effects), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_NRF54LM20B_SENSE);
    CHECK_EQ(state.next_slide_deadline_ms, 10000U);
    CHECK_EQ(state.language, 1U);
    CHECK(!xf_autoplay_due(&state, 9999U));
    CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, 10000U, &effects), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21);
    remote.product_id = 9U; remote.index = 1; remote.value = 0U;
    CHECK_EQ(xf_app_reduce(&state, &remote, &effects), XF_ERR_INVALID_ARG);
    snapshot.value = (int32_t)(UINT32_C(1) << 20);
    CHECK_EQ(xf_app_reduce(&state, &snapshot, &effects), XF_ERR_INVALID_ARG);

    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 11000U, &effects), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_SAMD21, 11100U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C6, 11200U), XF_OK);
    uint32_t selected = (UINT32_C(1) << XF_PRODUCT_SAMD21) | (UINT32_C(1) << XF_PRODUCT_ESP32_C6);
    CHECK_EQ(xf_app_displayed_products(&state), selected);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_COMPARE, 11300U, &effects), XF_OK);
    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_LANGUAGE, 11400U, &effects), XF_OK);
    CHECK_EQ(state.language, 0U);
    CHECK_EQ(state.screen, XF_SCREEN_COMPARE);
    CHECK_EQ(xf_app_displayed_products(&state), selected);
    CHECK_EQ(state.selected_count, 2U);

    /* Default English, round-trip every key, translated descriptions, proper
     * nouns untouched. This compiles the exact generated firmware tables. */
    CHECK_EQ(xf_string_language(), 0U);
    CHECK(strcmp(xf_language_button_text(), "中文") == 0);
    CHECK(strcmp(xf_string(XF_STRING_SCREEN_CATALOG_TITLE), "Products") == 0);
    for (unsigned id = 0; id < XF_STRING_COUNT; ++id) {
        xf_string_set_language(0U);
        const char *english = xf_string((xf_string_id_t)id);
        CHECK(english[0] != '\0');
        CHECK(xf_string_lookup(xf_string_key((xf_string_id_t)id)) == english);
        xf_string_set_language(1U);
        const char *chinese = xf_string((xf_string_id_t)id);
        CHECK(chinese[0] != '\0');
        CHECK(xf_string_lookup(xf_string_key((xf_string_id_t)id)) == chinese);
    }
    CHECK(strcmp(xf_language_button_text(), "EN") == 0);
    CHECK(strcmp(xf_string(XF_STRING_SCREEN_CATALOG_TITLE), "产品") == 0);
    CHECK(strcmp(xf_translate_text("Dual-band Wi-Fi 6"), "双频 Wi-Fi 6") == 0);
    CHECK(strcmp(xf_translate_text("XIAO RP2040"), "XIAO RP2040") == 0);
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        const xf_product_t *product = xf_product_find(id);
        CHECK(strcmp(xf_translate_text(product->notes), product->notes) != 0);
        CHECK(strcmp(xf_translate_text(product->applications), product->applications) != 0);
    }
    xf_string_set_language(0U);
    CHECK(strcmp(xf_translate_text("Dual-band Wi-Fi 6"), "Dual-band Wi-Fi 6") == 0);
    CHECK(xf_string_lookup("unknown") == NULL);
    CHECK(xf_string_lookup(NULL) == NULL);
    CHECK(strcmp(xf_string(XF_STRING_COUNT), "") == 0);
    xf_string_set_language(9U);
    CHECK_EQ(xf_string_language(), 0U);
}


/** The feedback waveform must end at silence, remain bounded and have no DC. */
static void test_feedback_tone(void)
{
    CHECK_EQ(XF_BEEP_SAMPLE_RATE_HZ, 16000U);
    CHECK_EQ(XF_BEEP_FREQUENCY_HZ, 600U);
    CHECK_EQ(XF_BEEP_DURATION_MS, 40U);
    CHECK_EQ(XF_BEEP_FRAMES, 640U);
    CHECK_EQ(xf_feedback_sample(0U), 0);
    CHECK_EQ(xf_feedback_sample(XF_BEEP_FRAMES - 1U), 0);
    CHECK_EQ(xf_feedback_sample(XF_BEEP_FRAMES), 0);
    CHECK_EQ(xf_feedback_sample(UINT32_MAX), 0);
    int32_t sum = 0;
    int32_t max_value = 0;
    int32_t previous = 0;
    uint32_t rising = 0U;
    for (uint32_t frame = 0; frame < XF_BEEP_FRAMES; ++frame) {
        int32_t sample = xf_feedback_sample(frame);
        int32_t magnitude = sample < 0 ? -sample : sample;
        CHECK(magnitude <= XF_BEEP_PEAK);
        if (magnitude > max_value) {
            max_value = magnitude;
        }
        if (previous <= 0 && sample > 0) {
            ++rising;
        }
        previous = sample;
        sum += sample;
    }
    CHECK_EQ(rising, 24U); /* 24 cycles / 0.040 s = 600 Hz. */
    CHECK_EQ(max_value, XF_BEEP_PEAK);
    CHECK(sum > -(int32_t)XF_BEEP_FRAMES && sum < (int32_t)XF_BEEP_FRAMES);
    /* At 600 Hz/16 kHz, 80 samples repeat exactly three cycles. */
    for (uint32_t n = 0; n < 32U; ++n) {
        CHECK_EQ(xf_feedback_sample(160U + n), xf_feedback_sample(240U + n));
    }
    CHECK(xf_feedback_sample(4U) < xf_feedback_sample(84U));
    int32_t fade = xf_feedback_sample(XF_BEEP_FRAMES - 12U);
    int32_t plateau = xf_feedback_sample(XF_BEEP_FRAMES - 92U);
    CHECK((fade < 0 ? -fade : fade) < (plateau < 0 ? -plateau : plateau));
}


/** A detail scroll belongs to one product, including transitions via autoplay. */
static void test_detail_scroll_ownership(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    start_attract(&state);
    CHECK_EQ(state.detail_product_id, XF_PRODUCT_INVALID);
    xf_event_t open = xf_event_make(XF_EVENT_OPEN_PRODUCT, 10U);
    open.product_id = XF_PRODUCT_SAMD21;
    CHECK_EQ(xf_app_reduce(&state, &open, &effects), XF_OK);
    CHECK(effects.reset_detail_scroll);
    CHECK_EQ(state.detail_product_id, XF_PRODUCT_SAMD21);
    state.return_context.scroll_y_px = 123;
    xf_event_t scroll = xf_event_make(XF_EVENT_SCROLL_STORE, 20U);
    scroll.index = XF_SCREEN_DETAIL;
    scroll.product_id = XF_PRODUCT_SAMD21;
    scroll.value = 900;
    CHECK_EQ(xf_app_reduce(&state, &scroll, &effects), XF_OK);
    CHECK_EQ(state.return_context.scroll_y_px, 123);
    CHECK(!effects.content_changed);
    CHECK_EQ(push(&state, XF_EVENT_TOGGLE_LANGUAGE, 25U, &effects), XF_OK);
    CHECK(!effects.reset_detail_scroll);
    CHECK_EQ(xf_app_reduce(&state, &open, &effects), XF_OK);
    CHECK(!effects.reset_detail_scroll);
    CHECK_EQ(push(&state, XF_EVENT_CLOSE_PAGE, 30U, &effects), XF_OK);
    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, 1, 40U), XF_OK);
    CHECK_EQ(state.current_product_id, XF_PRODUCT_SAMD21_PLUS);
    CHECK_EQ(state.detail_product_id, XF_PRODUCT_SAMD21);
    open.now_ms = 50U;
    open.product_id = XF_PRODUCT_SAMD21_PLUS;
    CHECK_EQ(xf_app_reduce(&state, &open, &effects), XF_OK);
    CHECK(effects.reset_detail_scroll);
    CHECK_EQ(state.detail_product_id, XF_PRODUCT_SAMD21_PLUS);
    /* A queued A scroll must not be applied to B or alter the Back offset. */
    CHECK_EQ(xf_app_reduce(&state, &scroll, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(state.return_context.scroll_y_px, 123);
    scroll.product_id = XF_PRODUCT_SAMD21_PLUS;
    CHECK_EQ(xf_app_reduce(&state, &scroll, &effects), XF_OK);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 60U, &effects), XF_OK);
    CHECK_EQ(xf_app_reduce(&state, &scroll, &effects), XF_ERR_INVALID_STATE);
    CHECK_EQ(xf_app_reduce(&state, &open, &effects), XF_OK);
    CHECK(!effects.reset_detail_scroll);
    xf_event_t remote = xf_event_make(XF_EVENT_REMOTE_TOUCH, 70U);
    remote.product_id = XF_PRODUCT_ESP32_C6;
    remote.index = 1;
    remote.value = (int32_t)(UINT32_C(1) << XF_PRODUCT_ESP32_C6);
    CHECK_EQ(xf_app_reduce(&state, &remote, &effects), XF_OK);
    CHECK(effects.reset_detail_scroll);
    CHECK_EQ(state.detail_product_id, XF_PRODUCT_ESP32_C6);
    remote.index = 0; remote.value = 0;
    CHECK_EQ(xf_app_reduce(&state, &remote, &effects), XF_OK);
    CHECK(!effects.reset_detail_scroll);
    state.notice = XF_NOTICE_SELECTION_READY;
    state.notice_until_ms = 100U;
    uint64_t last_input = state.last_input_release_ms;
    CHECK_EQ(push(&state, XF_EVENT_NOTICE_EXPIRE, 99U, &effects), XF_OK);
    CHECK(!effects.content_changed);
    CHECK_EQ(push(&state, XF_EVENT_NOTICE_EXPIRE, 100U, &effects), XF_OK);
    CHECK(effects.content_changed);
    CHECK_EQ(state.notice, XF_NOTICE_NONE);
    CHECK_EQ(state.last_input_release_ms, last_input);
    CHECK_EQ(state.return_context.scroll_y_px, 123);
}


/** Automatic LED-follow is optional; manual interactions stay active in both builds. */
static void test_autoplay_led_policy(void)
{
    xf_app_state_t state;
    xf_effects_t effects;
    xf_app_state_init(&state);
    CHECK_EQ(xf_app_host_selected_products(NULL), 0U);
    CHECK_EQ(xf_app_host_selected_products(&state), 0U);
    CHECK_EQ(push(&state, XF_EVENT_BOOT_READY, 0U, &effects), XF_OK);
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        CHECK_EQ(xf_app_displayed_products(&state), UINT32_C(1) << id);
#if XF_ENABLE_AUTOPLAY_LED_SYNC
        CHECK_EQ(xf_app_host_selected_products(&state), UINT32_C(1) << id);
#else
        CHECK_EQ(xf_app_host_selected_products(&state), 0U);
#endif
        CHECK_EQ(push(&state, XF_EVENT_SLIDE_DUE, (uint64_t)(id + 1U) * 3000U, &effects), XF_OK);
    }
    CHECK_EQ(push_index(&state, XF_EVENT_STEP_PRODUCT, 1, 55000U), XF_OK);
    CHECK(state.autoplay_paused);
    CHECK_EQ(xf_app_host_selected_products(&state), UINT32_C(1) << XF_PRODUCT_SAMD21_PLUS);
    CHECK_EQ(push_product(&state, XF_EVENT_OPEN_PRODUCT, XF_PRODUCT_ESP32_C6, 55100U), XF_OK);
    CHECK_EQ(xf_app_host_selected_products(&state), UINT32_C(1) << XF_PRODUCT_ESP32_C6);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_CATALOG, 55200U, &effects), XF_OK);
    CHECK_EQ(xf_app_host_selected_products(&state), 0U);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_SAMD21, 55300U), XF_OK);
    CHECK_EQ(push_product(&state, XF_EVENT_TOGGLE_COMPARE, XF_PRODUCT_ESP32_C6, 55400U), XF_OK);
    uint32_t compared = (UINT32_C(1) << XF_PRODUCT_SAMD21) | (UINT32_C(1) << XF_PRODUCT_ESP32_C6);
    CHECK_EQ(xf_app_host_selected_products(&state), compared);
    CHECK_EQ(push(&state, XF_EVENT_OPEN_COMPARE, 55500U, &effects), XF_OK);
    CHECK_EQ(xf_app_host_selected_products(&state), compared);
    CHECK_EQ(push(&state, XF_EVENT_IDLE_TIMEOUT, 61500U, &effects), XF_OK);
    CHECK(!state.autoplay_paused);
#if XF_ENABLE_AUTOPLAY_LED_SYNC
    CHECK_EQ(xf_app_host_selected_products(&state), UINT32_C(1) << state.current_product_id);
#else
    /* Clear the old manual/compare selection; don't leave a sticky LED on. */
    CHECK_EQ(xf_app_host_selected_products(&state), 0U);
#endif
    xf_event_t down = xf_event_make(XF_EVENT_REMOTE_TOUCH, 61600U);
    down.product_id = XF_PRODUCT_NRF54LM20A_SENSE;
    down.index = 1;
    down.value = (int32_t)(UINT32_C(1) << down.product_id);
    CHECK_EQ(xf_app_reduce(&state, &down, &effects), XF_OK);
    CHECK_EQ(xf_app_host_selected_products(&state), (uint32_t)down.value);
}

int main(void)
{
    test_autoplay_led_policy();
    test_detail_scroll_ownership();
    test_feedback_tone();
    test_touch_protocol();
    test_external_and_language();
    test_boot_and_carousel();
    test_press_pause_and_tap();
    test_drag_cancels_tap();
    test_manual_navigation();
    test_return_context();
    test_selection_set();
    test_compare_gate();
    test_differences_only_property();
    test_idle_recovery();
    test_catalogue_contract();
    test_argument_handling();

    printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures;
}