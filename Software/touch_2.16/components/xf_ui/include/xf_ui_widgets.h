/**
 * @file xf_ui_widgets.h
 * @brief Small, shared widget builders used by every page.
 *
 * Pages never position a raw `lv_obj_t`; they ask for a top bar, a body, a
 * card or a button. That keeps the hit areas, paddings and font sizes identical
 * across screens (design note 6.2) and makes the "no tap target below 80 px"
 * rule a single place to verify.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe content rectangle inside the native 480x480 rounded-square panel.
 *
 * The drawing does not dimension the active corner radius. A configurable
 * 24 px inset protects text/controls; it is not a claimed glass radius.
 * Background still covers all 480x480 pixels and touch coordinates stay native.
 */
#define XF_UI_PANEL_W 480
#define XF_UI_PANEL_H 480
#define XF_UI_SAFE_INSET 24
#define XF_UI_SCREEN_W (XF_UI_PANEL_W - 2 * XF_UI_SAFE_INSET)
#define XF_UI_SCREEN_H (XF_UI_PANEL_H - 2 * XF_UI_SAFE_INSET)
#define XF_UI_CONTENT_W (XF_UI_SCREEN_W - 24)
#define XF_UI_TOP_BAR_H 64
#define XF_UI_BOTTOM_BAR_H 72
#define XF_UI_ACTION_BAR_Y (XF_UI_SCREEN_H - XF_UI_BOTTOM_BAR_H)
#define XF_UI_NOTICE_H 28
#define XF_UI_NOTICE_Y (XF_UI_ACTION_BAR_Y - XF_UI_NOTICE_H)
#define XF_UI_PROGRESS_H 2
#define XF_UI_PROGRESS_Y (XF_UI_SCREEN_H - 4)
#define XF_UI_PROGRESS_W XF_UI_CONTENT_W

/** @brief Top bar split into the parts a page may extend. */
typedef struct {
    lv_obj_t *bar;      /**< The bar itself. */
    lv_obj_t *trailing; /**< Right-hand container for extra controls. */
    lv_obj_t *title;    /**< Title label, may be re-used as a status line. */
} xf_top_bar_t;

/**
 * @brief Create the page root: dark background, absolute children, no scroll.
 * @return New root object positioned inside the rounded-panel safe rectangle.
 */
lv_obj_t *xf_widget_root(void);

/** @brief Destination-language toggle; preserves page and selection state. */
lv_obj_t *xf_widget_language_button(lv_obj_t *parent);

/**
 * @brief Create the top bar.
 * @param[in] root Page root.
 * @param[in] title Already-translated title text (borrowed or copied by LVGL).
 * @param[in] with_back True to add the back button on the left.
 * @param[in] back_cb Click handler for the back button; may be NULL.
 * @param[in] back_user User pointer passed to @p back_cb.
 * @return Bar handle; `trailing` is NULL only when allocation failed.
 */
xf_top_bar_t xf_widget_top_bar(lv_obj_t *root, const char *title, bool with_back,
                               lv_event_cb_t back_cb, void *back_user);

/**
 * @brief Create the scrollable body between the top bar and the notice strip.
 * @param[in] root Page root.
 * @param[in] top Y coordinate of the body (normally XF_UI_TOP_BAR_H).
 * @param[in] height Visible height in px.
 * @return Body container; vertical scrolling is enabled.
 */
lv_obj_t *xf_widget_body(lv_obj_t *root, int32_t top, int32_t height);

/**
 * @brief Create the bottom action bar.
 * @param[in] root Page root.
 * @return Bar container with 16 px horizontal padding.
 */
lv_obj_t *xf_widget_action_bar(lv_obj_t *root);

/**
 * @brief Create a button with a fixed 44 px height and a 80 px minimum width.
 * @param[in] parent Container (usually the action bar).
 * @param[in] text Visible label; LVGL copies it.
 * @param[in] primary True for the brand-green call to action.
 * @param[in] cb Click handler; may be NULL.
 * @param[in] user User pointer passed to @p cb.
 * @return New button.
 */
lv_obj_t *xf_widget_button(lv_obj_t *parent, const char *text, bool primary,
                           lv_event_cb_t cb, void *user);

/**
 * @brief Create a flat (transparent) button, used for Back and filter chips.
 * @param[in] parent Container.
 * @param[in] text Visible label; LVGL copies it.
 * @param[in] cb Click handler; may be NULL.
 * @param[in] user User pointer passed to @p cb.
 * @return New button.
 */
lv_obj_t *xf_widget_flat_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                                void *user);

/**
 * @brief Create a single-line label.
 * @param[in] parent Container.
 * @param[in] text UTF-8 text; LVGL copies it.
 * @param[in] font Font to use; NULL keeps the inherited font.
 * @param[in] color Text colour; use LV_COLOR_MAKE(0,0,0) with @p use_color false.
 * @param[in] use_color True to apply @p color.
 * @return New label.
 */
lv_obj_t *xf_widget_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                          lv_color_t color, bool use_color);

/**
 * @brief Create a wrapping label of a fixed width.
 * @param[in] parent Container.
 * @param[in] text UTF-8 text; LVGL copies it.
 * @param[in] font Font to use; NULL keeps the inherited font.
 * @param[in] color Text colour.
 * @param[in] use_color True to apply @p color.
 * @param[in] width Fixed width in px; wrapping happens on spaces and '/'.
 * @return New label.
 */
lv_obj_t *xf_widget_wrapped_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                                  lv_color_t color, bool use_color, int32_t width);

/**
 * @brief Create the Seeed Studio logo on its light plate.
 * @param[in] parent Container.
 * @return Plate object containing the image, or NULL when the asset is missing.
 */
lv_obj_t *xf_widget_logo(lv_obj_t *parent);

/**
 * @brief Create a 1 px hairline that fills the parent width.
 * @param[in] parent Container.
 * @return New separator object.
 */
lv_obj_t *xf_widget_divider(lv_obj_t *parent);

/**
 * @brief Create one-line notice strip ("Up to 3 products", "Not verified").
 * @param[in] parent Container, usually placed above the action bar.
 * @param[in] text Notice text, or NULL to reserve no space.
 * @return Notice label, or NULL when @p text is NULL.
 */
lv_obj_t *xf_widget_notice(lv_obj_t *parent, const char *text);

#ifdef __cplusplus
}
#endif