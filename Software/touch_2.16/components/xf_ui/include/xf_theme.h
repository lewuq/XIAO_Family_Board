/**
 * @file xf_theme.h
 * @brief Colour tokens, shared styles and font accessors for every page.
 *
 * The values mirror `theme_tokens.json` from the design package; keeping them
 * in one place is what makes the sixteen pages look like one product instead of
 * sixteen experiments (design note 6.1). Nothing here allocates: the styles are
 * static and shared by reference, so a screen rebuild never grows the heap.
 */

#pragma once

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Colour tokens of the showcase theme. */
typedef struct {
    lv_color_t bg;            /**< Page background #081820. */
    lv_color_t card;          /**< Card surface #0D2E3D. */
    lv_color_t card_highlight;/**< Pressed / selected card fill. */
    lv_color_t text_primary;  /**< #F5F8F7 */
    lv_color_t text_secondary;/**< #B4C7CE, secondary lines and units. */
    lv_color_t brand_green;   /**< #8FC31F, brand accent and primary actions. */
    lv_color_t brand_teal;    /**< #004966, secondary surfaces. */
    lv_color_t on_green;      /**< #102028, text drawn on the green accent. */
    lv_color_t divider;       /**< #285366, hairlines between rows. */
    lv_color_t warning;       /**< Amber, used for "Not verified". */
    lv_color_t danger;        /**< Red, reserved for hard failures. */
    lv_color_t logo_bg;       /**< #F5F8F7 plate behind the logo artwork. */
} xf_theme_colors_t;

/**
 * @brief Build the static styles. Call once, inside LVGL context, before pages.
 * @note Safe to call twice: the second call is a no-op.
 */
void xf_theme_init(void);

/** @brief Read the colour tokens. Valid after xf_theme_init(). */
const xf_theme_colors_t *xf_theme(void);

/** Full-screen container: dark background, no padding, no scroll. */
lv_style_t *xf_style_screen(void);

/** Scrollable page body: transparent, vertical scroll, 16 px side padding. */
lv_style_t *xf_style_body(void);

/** Card at rest. Selected cards add a 2 px brand-green border. */
lv_style_t *xf_style_card(void);

/** Selected card decoration, applied on top of xf_style_card(). */
lv_style_t *xf_style_card_selected(void);

/** Primary action button: brand-green fill, dark text, 44 px high. */
lv_style_t *xf_style_button_primary(void);

/** Secondary action button: teal fill, light text, 44 px high. */
lv_style_t *xf_style_button_secondary(void);

/** Flat button used in the top bar (back / close). */
lv_style_t *xf_style_button_flat(void);

/** Filter chip at rest. */
lv_style_t *xf_style_chip(void);

/** Filter chip when active (brand green). */
lv_style_t *xf_style_chip_active(void);

/** Bottom action bar surface. */
lv_style_t *xf_style_action_bar(void);

/** 1 px hairline row separator. */
lv_style_t *xf_style_divider(void);

/** Round progress track and its brand-green indicator (carousel timer). */
lv_style_t *xf_style_progress_track(void);
lv_style_t *xf_style_progress_indicator(void);

/**
 * @brief Convert one of the theme colours into an lv_color_t.
 * @param[in] kind 0..11 index, useful for tests; out of range yields the text colour.
 * @return Borrowed colour value.
 */
lv_color_t xf_theme_color_at(unsigned kind);

#ifdef __cplusplus
}
#endif