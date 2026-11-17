/**
 * @file xf_ui_controller.c
 * @brief Serialised event loop: one queue, one task, one render path.
 *
 * Every interaction becomes an `xf_event_t` on a single queue. The task reduces
 * it with `xf_app_reduce()`, which returns the requested side effects, and the
 * renderer updates the current page from the resulting state. Two properties
 * follow and both matter for the acceptance cases:
 *
 *  - a tap and its release are applied in order, so a gesture can never be
 *    committed twice by two parallel paths (design note 7);
 *  - the picture is a function of the state, so a stale widget can never show a
 *    product that the state machine has already left (design note 10.4).
 */

#include "xf_ui.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_lvgl_port.h"

#include "xf_theme.h"
#include "xf_asset.h"
#include "xf_strings.h"
#include "xf_ui_internal.h"
#include "xf_ui_widgets.h"

/** Queue depth: a full second of taps plus timer events, then events are dropped. */
#define XF_UI_QUEUE_DEPTH 24
/** Stack: LVGL object creation is the deepest path, measured at ~2.4 KiB. */
#define XF_UI_TASK_STACK 6144
/** Below the LVGL task (priority 4) so a regenerate never starves the flush. */
#define XF_UI_TASK_PRIORITY 3
/** One scroll-store event per window; a flick emits hundreds. */
#define XF_UI_SCROLL_THROTTLE_MS 100U

static const char *TAG = "xf_ui";

static xf_app_state_t s_state;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static xf_ui_selection_sink_t s_selection_sink;
static void *s_selection_context;
static uint32_t s_notified_selection = UINT32_MAX;
/** Scrollable body of the page currently built; product changes reset its detail offset. */
static lv_obj_t *s_scrollable;
/** Screen the currently built tree belongs to (state.screen moves ahead of it). */
static uint8_t s_rendered_screen;
static uint8_t s_rendered_product_id;
static bool s_render_pending;
static bool s_reset_detail_scroll_pending;
/** Last scroll offset per screen: the visitor's place in the catalogue or a table. */
static int32_t s_scroll_memory[XF_SCREEN_COMPARE + 1];
static bool s_running;
static uint64_t s_scroll_post_ms;
static uint32_t s_dropped_events;

uint64_t xf_ui_now_ms(void)
{
    /* esp_timer is monotonic from boot; truncating to ms is exactly the unit the
     * state machine documents for every deadline. */
    return (uint64_t)(esp_timer_get_time() / 1000);
}

const xf_app_state_t *xf_ui_state(void)
{
    return &s_state;
}

void xf_ui_post(xf_event_type_t type, uint8_t product_id, int16_t index, int32_t value)
{
    if (s_queue == NULL) {
        return;
    }
    xf_event_t event = xf_event_make(type, xf_ui_now_ms());
    event.product_id = product_id;
    event.index = index;
    event.value = value;

    if (xQueueSend(s_queue, &event, 0) != pdTRUE) {
        /* Dropping is the documented failure mode: the alternative is blocking
         * the LVGL task inside a callback, which would stall the display. */
        if ((s_dropped_events++ % 32U) == 0U) {
            ESP_LOGW(TAG, "event queue full, dropped %u event(s) so far", (unsigned)s_dropped_events);
        }
    }
}

esp_err_t xf_ui_select_product(uint8_t product_id)
{
    if (xf_product_find(product_id) == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Task context only: adapters enqueue commands, never mutate LVGL here.
     * This entry point does not install an I2C slave or a bus protocol. */
    xf_event_t event = xf_event_make(XF_EVENT_OPEN_PRODUCT, xf_ui_now_ms());
    event.product_id = product_id;
    return xQueueSend(s_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t xf_ui_set_selection_sink(xf_ui_selection_sink_t sink, void *context)
{
    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    s_selection_sink = sink;
    s_selection_context = context;
    s_notified_selection = UINT32_MAX;
    return ESP_OK;
}

esp_err_t xf_ui_touchboard_event(uint8_t product_id, bool down, uint32_t held_products)
{
    if (product_id >= XF_PRODUCT_COUNT || (held_products >> XF_PRODUCT_COUNT) != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xf_event_t event = xf_event_make(XF_EVENT_REMOTE_TOUCH, xf_ui_now_ms());
    event.product_id = product_id;
    event.index = down ? 1 : 0;
    event.value = (int32_t)held_products;
    return xQueueSend(s_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t xf_ui_touchboard_snapshot(uint32_t held_products)
{
    if ((held_products >> XF_PRODUCT_COUNT) != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xf_event_t event = xf_event_make(XF_EVENT_REMOTE_STATE, xf_ui_now_ms());
    event.value = (int32_t)held_products;
    return xQueueSend(s_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void xf_ui_post_scroll(int32_t offset)
{
    uint64_t now = xf_ui_now_ms();
    if (now - s_scroll_post_ms < XF_UI_SCROLL_THROTTLE_MS) {
        return;
    }
    s_scroll_post_ms = now;
    xf_event_t event = xf_event_make(XF_EVENT_SCROLL_STORE, now);
    event.index = s_rendered_screen;
    event.product_id = s_rendered_product_id;
    event.value = offset;
    if (s_queue != NULL) {
        (void)xQueueSend(s_queue, &event, 0);
    }
}

/* ----- Gesture tracking -------------------------------------------------- */

/**
 * Press position of the gesture in progress.
 *
 * The gesture is tracked on the input device and not on a widget, because LVGL
 * only bubbles events to parents when the child asks for it, and a page's
 * buttons would otherwise swallow the press that the state machine needs to
 * know about. A tap is distinguished from a drag by comparing the press point
 * with the release point, which is the rule the design states (design note 7).
 */
static lv_point_t s_press_point;
/** Input device the listener is bound to, so binding happens exactly once. */
static lv_indev_t *s_indev;
static uint8_t s_press_screen;

bool xf_ui_gesture_is_tap(void)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return false;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    int32_t dx = point.x - s_press_point.x;
    int32_t dy = point.y - s_press_point.y;
    int32_t distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    return distance <= XF_TOUCH_SLOP_PX;
}

static void indev_pressed_cb(lv_event_t *event)
{
    (void)event;
    lv_indev_t *indev = lv_indev_active();
    if (indev != NULL) {
        lv_indev_get_point(indev, &s_press_point);
    }
    s_press_screen = s_rendered_screen;
    xf_ui_post(XF_EVENT_TOUCH_DOWN, XF_PRODUCT_INVALID, -1, 0);
}

static void indev_released_cb(lv_event_t *event)
{
    (void)event;
    int32_t distance = 0, dx = 0, dy = 0;
    lv_indev_t *indev = lv_indev_active();
    if (indev != NULL) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        dx = point.x - s_press_point.x;
        dy = point.y - s_press_point.y;
        distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    }
    if (distance > XF_TOUCH_SLOP_PX) {
        /* Report the cancelled tap before the release, so the reducer sees the
         * same order it would have seen from a frame-by-frame drag. */
        xf_ui_post(XF_EVENT_TOUCH_MOVE, XF_PRODUCT_INVALID, -1, distance);
    }
    xf_ui_post(XF_EVENT_TOUCH_UP, XF_PRODUCT_INVALID, -1, distance);
    int32_t abs_x = dx < 0 ? -dx : dx;
    int32_t abs_y = dy < 0 ? -dy : dy;
    if (s_press_screen == XF_SCREEN_ATTRACT && abs_x >= XF_SWIPE_THRESHOLD_PX &&
        abs_x > abs_y) {
        /* One release produces at most one step. Left swipe advances. */
        xf_ui_post(XF_EVENT_STEP_PRODUCT, XF_PRODUCT_INVALID, dx < 0 ? 1 : -1, 0);
    }
}

/** @brief Attach the gesture listener to the input device. LVGL lock held. */
static void bind_indev(void)
{
    if (s_indev != NULL) {
        return;
    }
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev == NULL) {
        /* The display is up but no input device is registered yet, which happens
         * when touch initialisation failed; the next render tries again. */
        return;
    }
    lv_indev_add_event_cb(indev, indev_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(indev, indev_released_cb, LV_EVENT_RELEASED, NULL);
    s_indev = indev;
}

/**
 * @brief One-shot report of the widget tree and the LVGL heap.
 *
 * Runs inside the renderer while the LVGL lock is held. It exists because a page
 * that is built but never painted looks identical to a page that was never built
 * when only the serial log is available: this line proves the tree exists, and
 * the heap figure shows whether LVGL had room to draw it.
 */
static void log_render_health_once(void)
{
    static bool logged;
    if (logged) {
        return;
    }
    logged = true;
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    ESP_LOGI(TAG, "first paint: screen=%u children=%u lvgl heap free=%u/%u bytes frag=%u%%",
             (unsigned)s_state.screen, (unsigned)lv_obj_get_child_count(lv_screen_active()),
             (unsigned)monitor.free_size, (unsigned)monitor.total_size, (unsigned)monitor.frag_pct);
}

/** @brief Commit one view: patch stable pages, rebuild when identity/layout requires it. */
static bool render(void)
{
    if (!lvgl_port_lock(UINT32_MAX)) {
        ESP_LOGE(TAG, "render skipped: LVGL lock unavailable");
        return false;
    }
    xf_string_set_language(s_state.language);
    xf_theme_init();
    bind_indev();

    /* Reuse stable widgets while only identity or selection/notice changes.
     * No root deletion, logo allocation or specification/image recreation. */
    if (!s_reset_detail_scroll_pending && s_rendered_screen == s_state.screen &&
        ((s_state.screen == XF_SCREEN_ATTRACT && xf_page_attract_update(&s_state)) ||
         (s_state.screen == XF_SCREEN_DETAIL && xf_page_detail_update(&s_state)))) {
        s_rendered_product_id = xf_app_visible_product(&s_state);
        lvgl_port_unlock();
        return true;
    }

    /* Capture the offset of the page that is about to be deleted before the
     * widget tree disappears; `s_state.screen` may already hold the next screen,
     * so the offset is filed under the screen that actually owned it. */
    if (s_scrollable != NULL) {
        s_scroll_memory[s_rendered_screen] = lv_obj_get_scroll_y(s_scrollable);
    }
    s_scrollable = NULL;
    /* Reset AFTER capturing the old tree: its offset must not overwrite the
     * new product's zero offset during this same rebuild. */
    if (s_reset_detail_scroll_pending) {
        s_scroll_memory[XF_SCREEN_DETAIL] = 0;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, xf_theme()->bg, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root = xf_widget_root();
    xf_page_result_t page = {0};

    switch ((xf_screen_t)s_state.screen) {
    case XF_SCREEN_ATTRACT:
        page = xf_page_attract_create(root, &s_state);
        break;
    case XF_SCREEN_CATALOG:
        page = xf_page_catalog_create(root, &s_state, false);
        break;
    case XF_SCREEN_SELECT:
        page = xf_page_catalog_create(root, &s_state, true);
        break;
    case XF_SCREEN_DETAIL:
        page = xf_page_detail_create(root, &s_state);
        break;
    case XF_SCREEN_COMPARE:
        page = xf_page_compare_create(root, &s_state);
        break;
    case XF_SCREEN_BOOT:
    default:
        xf_widget_label(root, xf_string(XF_STRING_SYSTEM_LOADING), xf_font_body(),
                        xf_theme()->text_secondary, true);
        break;
    }

    s_scrollable = page.scrollable;
    /* One opaque commit avoids repeated full-screen alpha redraws. */
    s_rendered_screen = s_state.screen;
    s_rendered_product_id = xf_app_visible_product(&s_state);

    /* Restore only the matching view's offset. Another detail product has been
     * reset above; language changes keep the current product's reading place. */
    if (page.scrollable != NULL && s_scroll_memory[s_state.screen] > 0) {
        lv_obj_scroll_to_y(page.scrollable, s_scroll_memory[s_state.screen], LV_ANIM_OFF);
    }

    log_render_health_once();
    lvgl_port_unlock();
    return true;
}

/** @brief Reduce one event and apply the requested side effects. */
static void apply_event(const xf_event_t *event)
{
    /* Discard delayed scroll callbacks from a previous screen/product. */
    if (event->type == XF_EVENT_SCROLL_STORE &&
        (event->index != s_state.screen ||
         (s_state.screen == XF_SCREEN_DETAIL && event->product_id != s_state.current_product_id))) {
        return;
    }
    uint8_t previous_notice = s_state.notice;
    xf_effects_t effects;
    xf_result_t result = xf_app_reduce(&s_state, event, &effects);

    if (result != XF_OK) {
        /* Rejections are part of normal use ("Up to 3 products"), so they are
         * logged at debug level instead of being reported as failures. */
        ESP_LOGD(TAG, "event %u rejected: %s", (unsigned)event->type, xf_result_name(result));
    }

    /* Any real input restarts the inactivity window. Timer events must not, or
     * the 6 s rule would never fire; a held finger must not either, so the
     * 6 s reset waits for the release like the design requires. */
    if (event->type != (uint8_t)XF_EVENT_SLIDE_DUE &&
        event->type != (uint8_t)XF_EVENT_IDLE_TIMEOUT &&
        event->type != (uint8_t)XF_EVENT_REMOTE_STATE &&
        event->type != (uint8_t)XF_EVENT_SCROLL_STORE &&
        event->type != (uint8_t)XF_EVENT_NOTICE_EXPIRE && !s_state.pointer_down) {
        s_state.last_input_release_ms = event->now_ms;
    }

    if (event->type == (uint8_t)XF_EVENT_SCROLL_STORE) {
        if (s_state.screen <= (uint8_t)XF_SCREEN_COMPARE) {
            s_scroll_memory[s_state.screen] = event->value;
        }
    }

    /* The idle rule drops the visitor state, so the remembered scroll positions
     * of that visit must not survive into the next one. */
    if (effects.clear_visitor) {
        for (size_t i = 0; i < sizeof(s_scroll_memory) / sizeof(s_scroll_memory[0]); i++) {
            s_scroll_memory[i] = 0;
        }
        s_scrollable = NULL;
    }

    s_reset_detail_scroll_pending |= effects.reset_detail_scroll;

    /* A rebuild while a finger is on the glass would delete the widget that is
     * currently receiving the gesture, so press and drag only update state. */
    if (event->type == (uint8_t)XF_EVENT_TOUCH_DOWN ||
        event->type == (uint8_t)XF_EVENT_TOUCH_MOVE) {
        if (previous_notice != s_state.notice) {
            s_render_pending = true;
        }
        return;
    }

    if (effects.screen_changed || effects.content_changed || effects.slide_committed) {
        s_render_pending = true;
    }
    uint32_t selected = xf_app_host_selected_products(&s_state);
    if (s_selection_sink != NULL && selected != s_notified_selection) {
        s_selection_sink(selected, s_selection_context);
        s_notified_selection = selected;
    }
}

/** @brief Evaluate the three deadlines that are not driven by input. */
static void tick(void)
{
    uint64_t now = xf_ui_now_ms();

    if (s_state.screen == (uint8_t)XF_SCREEN_ATTRACT && xf_autoplay_due(&s_state, now)) {
        xf_event_t event = xf_event_make(XF_EVENT_SLIDE_DUE, now);
        apply_event(&event);
    }

    if (xf_idle_expired(&s_state, now)) {
        xf_event_t event = xf_event_make(XF_EVENT_IDLE_TIMEOUT, now);
        apply_event(&event);
        return;
    }

    if (s_state.notice != (uint8_t)XF_NOTICE_NONE && s_state.notice_until_ms != 0U &&
        now >= s_state.notice_until_ms) {
        /* A timer expiry is not a scroll callback or new visitor input. */
        xf_event_t event = xf_event_make(XF_EVENT_NOTICE_EXPIRE, now);
        apply_event(&event);
        return;
    }

}

static void ui_task(void *argument)
{
    (void)argument;

    xf_event_t event = xf_event_make(XF_EVENT_BOOT_READY, xf_ui_now_ms());
    apply_event(&event);
    if (render()) {
        s_render_pending = false;
        s_reset_detail_scroll_pending = false;
    }
    ESP_LOGI(TAG, "controller running, screen=%u", (unsigned)s_state.screen);

    for (;;) {
        if (xQueueReceive(s_queue, &event, pdMS_TO_TICKS(XF_CONTROLLER_TICK_MS)) == pdTRUE) {
            apply_event(&event);
            /* Reduce a bounded burst once, then commit one coherent picture.
             * Press/release/click no longer allocate several trees in a row. */
            for (unsigned batch = 1; batch < 8U &&
                 xQueueReceive(s_queue, &event, 0) == pdTRUE; ++batch) {
                apply_event(&event);
            }
        }
        tick();
        if (s_render_pending && !s_state.pointer_down && render()) {
            s_render_pending = false;
            s_reset_detail_scroll_pending = false;
        }
    }
}

esp_err_t xf_ui_start(void)
{
    if (s_running) {
        return ESP_ERR_INVALID_STATE;
    }

    xf_app_state_init(&s_state);
    s_notified_selection = UINT32_MAX;
    s_scroll_post_ms = 0;
    s_scrollable = NULL;
    s_rendered_screen = (uint8_t)XF_SCREEN_BOOT;
    s_rendered_product_id = XF_PRODUCT_INVALID;
    s_render_pending = false;
    s_reset_detail_scroll_pending = false;
    for (size_t i = 0; i < sizeof(s_scroll_memory) / sizeof(s_scroll_memory[0]); i++) {
        s_scroll_memory[i] = 0;
    }

    s_queue = xQueueCreate(XF_UI_QUEUE_DEPTH, sizeof(xf_event_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "cannot create the event queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(ui_task, "xf_ui", XF_UI_TASK_STACK, NULL, XF_UI_TASK_PRIORITY, &s_task) !=
        pdPASS) {
        ESP_LOGE(TAG, "cannot create the controller task");
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_running = true;
    return ESP_OK;
}

esp_err_t xf_ui_stop(void)
{
    if (!s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    s_running = false;
    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_queue != NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
    s_scrollable = NULL;
    return ESP_OK;
}