/**
 * @file xf_event.h
 * @brief Business events posted by pages and the input adapter.
 *
 * Events are plain values: no LVGL pointers, no page-owned strings, so they can
 * be queued, copied and replayed (code rules 4.2). `now_ms` is always a
 * monotonic millisecond timestamp injected by the adapter; the reducer never
 * reads a clock itself.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Event kinds accepted by xf_app_reduce(). */
typedef enum {
    XF_EVENT_BOOT_READY = 0,      /**< First frame committed; enter ATTRACT. */
    XF_EVENT_SLIDE_DUE,           /**< Carousel deadline reached. */
    XF_EVENT_TOUCH_DOWN,          /**< Pointer pressed; freezes product identity. */
    XF_EVENT_TOUCH_UP,            /**< Pointer released; commits a tap when valid. */
    XF_EVENT_TOUCH_MOVE,          /**< Pointer moved beyond the slop distance. */
    XF_EVENT_OPEN_PRODUCT,        /**< Card or header tapped. */
    XF_EVENT_OPEN_CATALOG,        /**< "All products" navigation. */
    XF_EVENT_TOGGLE_COMPARE,      /**< Add or remove one product from the set. */
    XF_EVENT_OPEN_COMPARE,        /**< Enter COMPARE (needs 2 or 3 products). */
    XF_EVENT_EDIT_SELECTION,      /**< Enter SELECT from COMPARE. */
    XF_EVENT_CLEAR_SELECTION,     /**< "Clear all". */
    XF_EVENT_SET_FILTER,          /**< Manufacturer filter changed. */
    XF_EVENT_SET_COMPARE_CATEGORY,/**< COMPARE tab changed. */
    XF_EVENT_TOGGLE_DIFFERENCES,  /**< "Differences only" switch. */
    XF_EVENT_CLOSE_PAGE,          /**< Back navigation. */
    XF_EVENT_SCROLL_STORE,        /**< Page scrolled; remember the offset. */
    XF_EVENT_IDLE_TIMEOUT,        /**< 6 s without input; return to ATTRACT. */
    XF_EVENT_ASSET_ERROR,         /**< Hero image of a product is unusable. */
    XF_EVENT_STEP_PRODUCT,        /**< ATTRACT previous/next; index is -1 or +1. */
    XF_EVENT_TOGGLE_AUTOPLAY,     /**< Retained task/API pause/resume event. */
    XF_EVENT_TOGGLE_LANGUAGE,     /**< Switch English/Chinese without changing selection. */
    XF_EVENT_REMOTE_TOUCH,        /**< Board DOWN/UP, product_id + held product mask. */
    XF_EVENT_NOTICE_EXPIRE,       /**< Timer-only notice expiry; never changes scroll/input time. */
    XF_EVENT_REMOTE_STATE,        /**< Recovery snapshot of the board's held product mask. */
} xf_event_type_t;

/** @brief One immutable business event. */
typedef struct {
    uint8_t type;        /**< xf_event_type_t. */
    uint8_t product_id;  /**< Subject product; XF_PRODUCT_INVALID when unused. */
    int16_t index;       /**< Filter index, category, or -1 when unused. */
    int32_t value;       /**< Drag distance (px) or stored scroll offset (px). */
    uint64_t now_ms;     /**< Monotonic timestamp of the input, in ms. */
} xf_event_t;

/**
 * @brief Build an event with neutral defaults.
 * @param[in] type Event kind.
 * @param[in] now_ms Monotonic timestamp in milliseconds.
 * @return Value-initialised event; caller fills the fields it needs.
 */
static inline xf_event_t xf_event_make(xf_event_type_t type, uint64_t now_ms)
{
    xf_event_t event;
    event.type = (uint8_t)type;
    event.product_id = 0xFFU;
    event.index = -1;
    event.value = 0;
    event.now_ms = now_ms;
    return event;
}

#ifdef __cplusplus
}
#endif