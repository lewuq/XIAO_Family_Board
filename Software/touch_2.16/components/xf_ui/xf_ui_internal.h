/**
 * @file xf_ui_internal.h
 * @brief Contract between the controller and the five page modules.
 *
 * A page is a pure function of the business state: it receives the state, builds
 * widgets and returns its scrollable body. Cached widget handles support partial
 * updates; the controller owns business data, so a stale
 * pointer can never resurrect an outdated selection (design note 10.4).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "xf_app_state.h"
#include "xf_ui_widgets.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Objects a page hands back to the controller. */
typedef struct {
    lv_obj_t *scrollable; /**< Body whose scroll offset must be restored; may be NULL. */
} xf_page_result_t;

/* ------------------------------------------------------------------------- */
/* Services the controller offers to pages                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Monotonic milliseconds since boot.
 * @return Timestamp suitable for `xf_event_t.now_ms`.
 */
uint64_t xf_ui_now_ms(void);

/** @brief Check the current physical gesture before a touch click navigates. */
bool xf_ui_gesture_is_tap(void);

/**
 * @brief Post one business event to the controller queue.
 * @param[in] type xf_event_type_t value.
 * @param[in] product_id Subject product, or XF_PRODUCT_INVALID.
 * @param[in] index Filter index / compare category, or -1.
 * @param[in] value Drag distance or scroll offset.
 * @note Non-blocking: a full queue drops the event and logs at debug level, so
 *       an input burst can never stall the LVGL task (design note 10.4).
 */
void xf_ui_post(xf_event_type_t type, uint8_t product_id, int16_t index, int32_t value);

/**
 * @brief Store the vertical scroll offset of the page being left.
 * @param[in] offset Current scroll position in px.
 * @note Throttled to one event per XF_UI_SCROLL_THROTTLE_MS, because LVGL emits
 *       a scroll event for every pixel of a flick.
 */
void xf_ui_post_scroll(int32_t offset);

/**
 * @brief Read the state the pages must render.
 * @return Borrowed pointer owned by the controller; never NULL.
 */
const xf_app_state_t *xf_ui_state(void);

/* ------------------------------------------------------------------------- */
/* Pages                                                                      */
/* ------------------------------------------------------------------------- */

/**
 * @brief Build ATTRACT: logo, hero image, dots, language toggle and browse entry.
 * @param[in] root Container created by xf_widget_root().
 * @param[in] state Snapshot to render.
 * @return Zero-initialised result; ATTRACT never scrolls.
 */
xf_page_result_t xf_page_attract_create(lv_obj_t *root, const xf_app_state_t *state);
/** @brief Update the existing carousel widgets; LVGL lock held. */
bool xf_page_attract_update(const xf_app_state_t *state);

/**
 * @brief Build CATALOG or SELECT: filter chips, two-column cards, action bar.
 * @param[in] root Container created by xf_widget_root().
 * @param[in] state Snapshot to render.
 * @param[in] select_mode True for SELECT semantics (tap toggles the set).
 * @return Result with `scrollable` set to the card area.
 */
xf_page_result_t xf_page_catalog_create(lv_obj_t *root, const xf_app_state_t *state,
                                        bool select_mode);

/**
 * @brief Build DETAIL: hero, identity, grouped specs, applications, notes.
 * @param[in] root Container created by xf_widget_root().
 * @param[in] state Snapshot to render.
 * @return Result with `scrollable` set to the content area.
 */
xf_page_result_t xf_page_detail_create(lv_obj_t *root, const xf_app_state_t *state);
/** @brief Update only notice/actions for the same product/language; lock held. */
bool xf_page_detail_update(const xf_app_state_t *state);

/**
 * @brief Build COMPARE: fixed product header, category tabs, field table.
 * @param[in] root Container created by xf_widget_root().
 * @param[in] state Snapshot to render; `selected_count` is 2 or 3.
 * @return Result with `scrollable` set to the table area.
 */
xf_page_result_t xf_page_compare_create(lv_obj_t *root, const xf_app_state_t *state);

/**
 * @brief Create the notice strip above the action bar.
 * @param[in] root Container created by xf_widget_root().
 * @param[in] state Snapshot to render.
 * @return Height consumed in px (0 when no notice is visible), so the page can
 *         shrink its body by exactly that amount.
 */
int32_t xf_page_attach_notice(lv_obj_t *root, const xf_app_state_t *state);

#ifdef __cplusplus
}
#endif