/**
 * @file xf_state_reducer.c
 * @brief Deterministic event -> state reduction for every screen transition.
 *
 * The reducer is the only place that decides navigation, selection changes and
 * timer re-arming. It returns requested side effects instead of touching the
 * UI, so the controller can apply them in LVGL context and the rules stay
 * testable without a display (code rules 4.2).
 */

#include <stddef.h>

#include "xf_app_state.h"

/**
 * @brief Arm a transient notice.
 * @param[in,out] state Current state.
 * @param[in] notice Notice to show.
 * @param[in] now_ms Monotonic timestamp in ms.
 */
static void set_notice(xf_app_state_t *state, xf_notice_t notice, uint64_t now_ms)
{
    state->notice = (uint8_t)notice;
    state->notice_until_ms = now_ms + XF_NOTICE_DURATION_MS;
}

/** @brief Remember where the user came from before leaving `screen`. */
static void save_return_context(xf_app_state_t *state)
{
    state->return_context.screen = state->screen;
    state->return_context.filter = state->manufacturer_filter;
    state->return_context.category = state->compare_category;
    /* Scroll offsets are pushed by the pages through XF_EVENT_SCROLL_STORE, so
     * the stored value always belongs to the page being left. */
}

/** @brief Notice text for the current selection size. */
static xf_notice_t selection_notice(const xf_app_state_t *state)
{
    if (state->selected_count == 0U) {
        return XF_NOTICE_SELECTION_EMPTY;
    }
    if (!xf_compare_ready(state)) {
        return XF_NOTICE_SELECTION_MIN;
    }
    return XF_NOTICE_SELECTION_READY;
}

void xf_app_state_init(xf_app_state_t *state)
{
    if (state == NULL) {
        return;
    }
    *state = (xf_app_state_t){0};
    state->screen = XF_SCREEN_BOOT;
    state->current_product_id = XF_PRODUCT_INVALID;
    state->detail_product_id = XF_PRODUCT_INVALID;
    state->interaction_product_id = XF_PRODUCT_INVALID;
    state->manufacturer_filter = -1;
    state->compare_category = XF_COMPARE_CATEGORY_COMPUTE;
    state->return_context.screen = XF_SCREEN_ATTRACT;
    state->return_context.filter = -1;
}

uint8_t xf_app_visible_product(const xf_app_state_t *state)
{
    if (state == NULL || state->current_product_id >= XF_PRODUCT_COUNT) {
        return XF_PRODUCT_INVALID;
    }
    return state->current_product_id;
}

uint32_t xf_app_displayed_products(const xf_app_state_t *state)
{
    if (state == NULL) {
        return 0U;
    }
    uint32_t mask = 0U;
    if (state->screen == XF_SCREEN_SELECT || state->screen == XF_SCREEN_COMPARE ||
        state->screen == XF_SCREEN_CATALOG) {
        for (uint8_t i = 0; i < state->selected_count && i < XF_COMPARE_CAPACITY; ++i) {
            if (state->selected_ids[i] < XF_PRODUCT_COUNT) {
                mask |= UINT32_C(1) << state->selected_ids[i];
            }
        }
    } else if (state->current_product_id < XF_PRODUCT_COUNT) {
        mask = UINT32_C(1) << state->current_product_id;
    }
    return mask;
}

uint32_t xf_app_host_selected_products(const xf_app_state_t *state)
{
    if (state == NULL || state->screen == XF_SCREEN_BOOT) {
        return 0U;
    }
#if !XF_ENABLE_AUTOPLAY_LED_SYNC
    if (state->screen == XF_SCREEN_ATTRACT && !state->autoplay_paused) {
        /* Notify zero on manual -> autoplay transition. The controller already
         * suppresses duplicate masks, so later slides do not repeat the clear. */
        return 0U;
    }
#endif
    return xf_app_displayed_products(state);
}

static xf_result_t reduce_touch_down(xf_app_state_t *state, const xf_event_t *event,
                                     xf_effects_t *effects)
{
    if (state->pointer_down) {
        /* A second press without a release is a lost report, not a new tap:
         * drop it so one gesture can never commit twice (design note 7). */
        return XF_ERR_INVALID_STATE;
    }
    /* The bar value must be sampled before `pointer_down` is raised: the
     * progress accessor answers with the frozen value while a press is active,
     * so reading it afterwards would freeze the previous hold instead.
     */
    uint16_t held_permille = 0U;
    if (state->screen == XF_SCREEN_ATTRACT) {
        held_permille = xf_autoplay_progress_permille(state, event->now_ms);
    }

    state->pointer_down = true;
    state->tap_cancelled = false;
    /* Freeze the identity that is actually on screen right now. */
    state->interaction_product_id = xf_app_visible_product(state);

    if (state->screen == XF_SCREEN_ATTRACT) {
        /* A touch pauses until six seconds after release, including a swipe
         * that does not open a different page. No progress widget is rendered. */
        state->slide_hold_permille = held_permille;
        state->autoplay_paused = true;
        state->next_slide_deadline_ms = 0U;
    }
    effects->content_changed = true;
    return XF_OK;
}

static xf_result_t reduce_touch_move(xf_app_state_t *state, const xf_event_t *event,
                                     xf_effects_t *effects)
{
    if (!state->pointer_down) {
        return XF_ERR_INVALID_STATE;
    }
    int32_t distance = event->value < 0 ? -event->value : event->value;
    if (distance > XF_TOUCH_SLOP_PX) {
        state->tap_cancelled = true;
    }
    effects->content_changed = false;
    return XF_OK;
}

static xf_result_t reduce_touch_up(xf_app_state_t *state, const xf_event_t *event,
                                   xf_effects_t *effects)
{
    if (!state->pointer_down) {
        return XF_ERR_INVALID_STATE;
    }
    state->pointer_down = false;
    state->last_input_release_ms = event->now_ms;

    if (state->screen == XF_SCREEN_ATTRACT) {
        /* The clicked widget decides navigation; release alone must not
         * intercept Browse or language switching before their click handlers run. */
        xf_autoplay_restart(state, event->now_ms);
        effects->reset_slide_clock = true;
        /* No page rebuild until the click/swipe is resolved. */
        effects->content_changed = false;
    }
    return XF_OK;
}

static xf_result_t reduce_open_product(xf_app_state_t *state, uint8_t product_id,
                                       xf_effects_t *effects)
{
    if (xf_product_find(product_id) == NULL) {
        return XF_ERR_INVALID_ID;
    }
    if (state->screen != XF_SCREEN_CATALOG && state->screen != XF_SCREEN_SELECT &&
        state->screen != XF_SCREEN_COMPARE && state->screen != XF_SCREEN_ATTRACT &&
        state->screen != XF_SCREEN_DETAIL) {
        return XF_ERR_INVALID_STATE;
    }
    /* Changing an open model keeps the original Back destination. */
    if (state->screen != XF_SCREEN_DETAIL) {
        save_return_context(state);
    }
    if (state->detail_product_id != product_id) {
        state->detail_product_id = product_id;
        effects->reset_detail_scroll = true;
    }
    state->current_product_id = product_id;
    state->screen = XF_SCREEN_DETAIL;
    state->next_slide_deadline_ms = 0U;
    effects->screen_changed = true;
    return XF_OK;
}

static xf_result_t reduce_toggle_compare(xf_app_state_t *state, uint8_t product_id,
                                         const xf_event_t *event, xf_effects_t *effects)
{
    xf_result_t result = xf_compare_toggle(state, product_id);

    if (result == XF_ERR_COMPARE_FULL) {
        /* The rejected product must leave the three existing columns untouched,
         * so the set is never written before the capacity check. */
        set_notice(state, XF_NOTICE_SELECTION_MAX, event->now_ms);
        effects->content_changed = true;
        return result;
    }
    if (result != XF_OK) {
        return result;
    }
    set_notice(state, selection_notice(state), event->now_ms);
    /* Selected decorations are derived from the state by the catalog page, so a
     * selection change only repaints content, it never rebuilds the tree. */
    effects->content_changed = true;
    return XF_OK;
}

static xf_result_t reduce_idle(xf_app_state_t *state, const xf_event_t *event,
                               xf_effects_t *effects)
{
    if (state->screen == XF_SCREEN_BOOT ||
        (state->screen == XF_SCREEN_ATTRACT && !state->autoplay_paused)) {
        return XF_ERR_INVALID_STATE;
    }
    if (state->pointer_down || state->external_touch_mask != 0U) {
        return XF_ERR_INVALID_STATE;
    }
    /* The visitor state is intentionally not persisted: selection, filter and
     * the return stack are dropped on the way back to the carousel
     * (design note 7 and 8.8). */
    xf_compare_clear(state);
    state->manufacturer_filter = -1;
    state->compare_category = XF_COMPARE_CATEGORY_COMPUTE;
    state->compare_differences_only = false;
    state->return_context.screen = XF_SCREEN_ATTRACT;
    state->return_context.filter = -1;
    state->return_context.scroll_y_px = 0;
    state->notice = XF_NOTICE_NONE;
    state->notice_until_ms = 0U;

    state->screen = XF_SCREEN_ATTRACT;
    state->detail_product_id = XF_PRODUCT_INVALID;
    state->autoplay_paused = false;
    state->autoplay_index = xf_app_visible_product(state) < XF_PRODUCT_COUNT
                            ? state->current_product_id : 0U;
    state->current_product_id = state->autoplay_index;
    /* First automatic change is at the 6-second idle boundary, then every
     * 3 seconds. Do not add another slide period to the idle delay. */
    state->next_slide_deadline_ms = event->now_ms;
    xf_autoplay_commit(state, event->now_ms);

    effects->screen_changed = true;
    effects->clear_visitor = true;
    effects->reset_slide_clock = true;
    return XF_OK;
}

xf_result_t xf_app_reduce(xf_app_state_t *state, const xf_event_t *event, xf_effects_t *effects)
{
    if (state == NULL || event == NULL || effects == NULL) {
        return XF_ERR_INVALID_ARG;
    }
    *effects = (xf_effects_t){0};
    effects->result = XF_OK;

    xf_result_t result = XF_OK;

    switch ((xf_event_type_t)event->type) {
    case XF_EVENT_BOOT_READY:
        state->screen = XF_SCREEN_ATTRACT;
        state->current_product_id = 0U;
        state->autoplay_index = 0U;
        state->last_input_release_ms = event->now_ms;
        state->next_slide_deadline_ms = event->now_ms + XF_AUTOPLAY_INTERVAL_MS;
        effects->screen_changed = true;
        effects->reset_slide_clock = true;
        break;

    case XF_EVENT_TOGGLE_LANGUAGE:
        state->language = state->language == 0U ? 1U : 0U;
        effects->content_changed = true;
        break;

    case XF_EVENT_REMOTE_TOUCH:
    case XF_EVENT_REMOTE_STATE: {
        const uint32_t valid_mask = (UINT32_C(1) << XF_PRODUCT_COUNT) - 1U;
        uint32_t held = (uint32_t)event->value;
        if ((held & ~valid_mask) != 0U ||
            (event->type == XF_EVENT_REMOTE_TOUCH &&
             (event->product_id >= XF_PRODUCT_COUNT || (event->index != 0 && event->index != 1)))) {
            result = XF_ERR_INVALID_ARG;
            break;
        }
        if (event->type == XF_EVENT_REMOTE_STATE && held == state->external_touch_mask) {
            break; /* Polling an unchanged bitmap is not a new user touch. */
        }
        if (event->type == XF_EVENT_REMOTE_TOUCH &&
            (((held >> event->product_id) & 1U) != (uint32_t)event->index)) {
            result = XF_ERR_INVALID_ARG;
            break;
        }
        state->external_touch_mask = held;
        state->last_input_release_ms = event->now_ms;
        state->autoplay_paused = true;
        state->next_slide_deadline_ms = 0U;
        uint8_t target = XF_PRODUCT_INVALID;
        if (event->type == XF_EVENT_REMOTE_TOUCH && event->index == 1) {
            target = event->product_id;
        } else if (event->type == XF_EVENT_REMOTE_STATE && held != 0U) {
            /* Recovery selects one currently held model; it cannot recreate
             * historical ordering lost in an overflow. */
            for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
                if ((held & (UINT32_C(1) << id)) != 0U) {
                    target = id;
                }
            }
        }
        if (target < XF_PRODUCT_COUNT) {
            result = reduce_open_product(state, target, effects);
        }
        break;
    }

    case XF_EVENT_STEP_PRODUCT:
        if (state->screen != XF_SCREEN_ATTRACT) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        if (event->index != -1 && event->index != 1) {
            result = XF_ERR_INVALID_ARG;
            break;
        }
        state->autoplay_paused = true;
        state->current_product_id = event->index < 0
                                      ? xf_product_prev(state->current_product_id)
                                      : xf_product_next(state->current_product_id);
        state->autoplay_index = state->current_product_id;
        state->next_slide_deadline_ms = 0U;
        state->slide_hold_permille = 0U;
        state->last_input_release_ms = event->now_ms;
        effects->slide_committed = true;
        break;

    case XF_EVENT_TOGGLE_AUTOPLAY:
        if (state->screen != XF_SCREEN_ATTRACT) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        state->autoplay_paused = !state->autoplay_paused;
        state->last_input_release_ms = event->now_ms;
        state->slide_hold_permille = 0U;
        xf_autoplay_restart(state, event->now_ms);
        effects->content_changed = true;
        break;

    case XF_EVENT_SLIDE_DUE:
        if (!xf_autoplay_due(state, event->now_ms)) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        xf_autoplay_commit(state, event->now_ms);
        effects->slide_committed = true;
        break;

    case XF_EVENT_TOUCH_DOWN:
        result = reduce_touch_down(state, event, effects);
        break;

    case XF_EVENT_TOUCH_MOVE:
        result = reduce_touch_move(state, event, effects);
        break;

    case XF_EVENT_TOUCH_UP:
        result = reduce_touch_up(state, event, effects);
        break;

    case XF_EVENT_OPEN_PRODUCT:
        result = reduce_open_product(state, event->product_id, effects);
        break;

    case XF_EVENT_OPEN_CATALOG:
        if (state->screen != XF_SCREEN_DETAIL && state->screen != XF_SCREEN_ATTRACT &&
            state->screen != XF_SCREEN_SELECT) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        save_return_context(state);
        state->screen = XF_SCREEN_CATALOG;
        state->next_slide_deadline_ms = 0U;
        effects->screen_changed = true;
        break;

    case XF_EVENT_TOGGLE_COMPARE:
        result = reduce_toggle_compare(state, event->product_id, event, effects);
        break;

    case XF_EVENT_OPEN_COMPARE:
        if (!xf_compare_ready(state)) {
            set_notice(state, XF_NOTICE_SELECTION_MIN, event->now_ms);
            effects->content_changed = true;
            result = XF_ERR_COMPARE_TOO_FEW;
            break;
        }
        if (state->screen != XF_SCREEN_DETAIL && state->screen != XF_SCREEN_SELECT &&
            state->screen != XF_SCREEN_CATALOG) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        save_return_context(state);
        state->screen = XF_SCREEN_COMPARE;
        effects->screen_changed = true;
        break;

    case XF_EVENT_EDIT_SELECTION:
        /* COMPARE -> SELECT is the "Edit" entry. CATALOG -> SELECT is the
         * "Select products" entry of the catalogue action bar; both land on the
         * same grid with selection semantics (design note 5). */
        if (state->screen != XF_SCREEN_COMPARE && state->screen != XF_SCREEN_CATALOG) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        state->screen = XF_SCREEN_SELECT;
        effects->screen_changed = true;
        break;

    case XF_EVENT_CLEAR_SELECTION:
        xf_compare_clear(state);
        set_notice(state, XF_NOTICE_SELECTION_EMPTY, event->now_ms);
        effects->content_changed = true;
        break;

    case XF_EVENT_SET_FILTER:
        if (event->index < -1 || event->index >= XF_MANUFACTURER_COUNT) {
            result = XF_ERR_INVALID_ARG;
            break;
        }
        /* Filtering never touches the selection set: selected products stay
         * selected even when they are hidden (design note 7). */
        state->manufacturer_filter = (int8_t)event->index;
        effects->content_changed = true;
        break;

    case XF_EVENT_SET_COMPARE_CATEGORY:
        if (event->index < 0 || event->index >= XF_COMPARE_CATEGORY_COUNT) {
            result = XF_ERR_INVALID_ARG;
            break;
        }
        state->compare_category = (uint8_t)event->index;
        effects->content_changed = true;
        break;

    case XF_EVENT_TOGGLE_DIFFERENCES:
        state->compare_differences_only = !state->compare_differences_only;
        effects->content_changed = true;
        break;

    case XF_EVENT_CLOSE_PAGE:
        switch ((xf_screen_t)state->screen) {
        case XF_SCREEN_DETAIL:
        case XF_SCREEN_COMPARE:
            state->screen = state->return_context.screen;
            if (state->screen == XF_SCREEN_BOOT) {
                state->screen = XF_SCREEN_ATTRACT;
            }
            if (state->screen == XF_SCREEN_COMPARE && !xf_compare_ready(state)) {
                /* A comparison that lost a column returns to SELECT instead of
                 * rendering a single-product table (design note 7). */
                state->screen = XF_SCREEN_SELECT;
            }
            effects->screen_changed = true;
            break;
        case XF_SCREEN_SELECT:
            state->screen = XF_SCREEN_CATALOG;
            effects->screen_changed = true;
            break;
        case XF_SCREEN_CATALOG:
            state->screen = XF_SCREEN_ATTRACT;
            effects->screen_changed = true;
            break;
        default:
            result = XF_ERR_INVALID_STATE;
            break;
        }
        /* Whichever path led back to the carousel, it resumes with a full slot.
         * DETAIL/COMPARE zeroed the deadline on entry, so without this the
         * carousel would stay frozen after a single product visit. */
        if (result == XF_OK && state->screen == XF_SCREEN_ATTRACT) {
            state->slide_hold_permille = 0U;
            xf_autoplay_restart(state, event->now_ms);
            effects->reset_slide_clock = true;
        }
        break;

    case XF_EVENT_SCROLL_STORE:
        if (event->index >= 0 &&
            (event->index != state->screen ||
             (state->screen == XF_SCREEN_DETAIL && event->product_id != state->current_product_id))) {
            result = XF_ERR_INVALID_STATE;
            break;
        }
        /* Detail offsets belong to the controller's product-owned view, not
         * the Back destination's catalogue/compare offset. */
        if (state->screen != XF_SCREEN_DETAIL) {
            state->return_context.scroll_y_px = event->value;
        }
        break;

    case XF_EVENT_NOTICE_EXPIRE:
        break; /* The common tail expires the notice without fake scrolling. */

    case XF_EVENT_IDLE_TIMEOUT:
        result = reduce_idle(state, event, effects);
        break;

    case XF_EVENT_ASSET_ERROR:
        set_notice(state, XF_NOTICE_IMAGE_UNAVAILABLE, event->now_ms);
        effects->content_changed = true;
        break;

    default:
        result = XF_ERR_INVALID_ARG;
        break;
    }

    if (state->notice != XF_NOTICE_NONE && !effects->screen_changed &&
        state->notice_until_ms != 0U && event->now_ms >= state->notice_until_ms) {
        state->notice = XF_NOTICE_NONE;
        state->notice_until_ms = 0U;
        effects->content_changed = true;
    }

    effects->result = result;
    return result;
}