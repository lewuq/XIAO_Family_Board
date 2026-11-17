/**
 * @file xf_theme.c
 * @brief Static style set shared by all pages.
 *
 * Styles are created once and attached by pointer. LVGL 9 keeps the reference
 * count, so an object deletion frees only the object, never the style; this is
 * what allows the declarative "destroy and rebuild the page" renderer without
 * leaking a byte per interaction (design note 10.3).
 */

#include "xf_theme.h"

#include <stddef.h>

#include "xf_asset.h"

/** Colour tokens; the numeric values live only in this table. */
static const xf_theme_colors_t s_colors = {
    .bg = LV_COLOR_MAKE(0x08, 0x18, 0x20),
    .card = LV_COLOR_MAKE(0x0D, 0x2E, 0x3D),
    .card_highlight = LV_COLOR_MAKE(0x00, 0x49, 0x66), /* theme_tokens surface_active */
    .text_primary = LV_COLOR_MAKE(0xF5, 0xF8, 0xF7),
    .text_secondary = LV_COLOR_MAKE(0xB4, 0xC7, 0xCE),
    .brand_green = LV_COLOR_MAKE(0x8F, 0xC3, 0x1F),
    .brand_teal = LV_COLOR_MAKE(0x00, 0x49, 0x66),
    .on_green = LV_COLOR_MAKE(0x10, 0x20, 0x28),
    .divider = LV_COLOR_MAKE(0x28, 0x53, 0x66),
    .warning = LV_COLOR_MAKE(0xF0, 0xB3, 0x4E), /* theme_tokens warning */
    .danger = LV_COLOR_MAKE(0xE5, 0x8F, 0x87),  /* theme_tokens error */
    .logo_bg = LV_COLOR_MAKE(0xF5, 0xF8, 0xF7),
};

static bool s_ready = false;

static lv_style_t s_screen;
static lv_style_t s_body;
static lv_style_t s_card;
static lv_style_t s_card_selected;
static lv_style_t s_button_primary;
static lv_style_t s_button_secondary;
static lv_style_t s_button_flat;
static lv_style_t s_chip;
static lv_style_t s_chip_active;
static lv_style_t s_action_bar;
static lv_style_t s_divider;
static lv_style_t s_progress_track;
static lv_style_t s_progress_indicator;

const xf_theme_colors_t *xf_theme(void)
{
    return &s_colors;
}

lv_color_t xf_theme_color_at(unsigned kind)
{
    /* A switch, not pointer arithmetic: the token struct may contain padding,
     * so `&s_colors.bg + kind` would not be the field the caller asked for. */
    switch (kind) {
    case 0: return s_colors.bg;
    case 1: return s_colors.card;
    case 2: return s_colors.card_highlight;
    case 3: return s_colors.text_primary;
    case 4: return s_colors.text_secondary;
    case 5: return s_colors.brand_green;
    case 6: return s_colors.brand_teal;
    case 7: return s_colors.on_green;
    case 8: return s_colors.divider;
    case 9: return s_colors.warning;
    case 10: return s_colors.danger;
    case 11: return s_colors.logo_bg;
    default: return s_colors.text_primary;
    }
}

void xf_theme_init(void)
{
    if (s_ready) {
        return;
    }
    s_ready = true;

    const lv_font_t *body = xf_font_body();
    const lv_font_t *dense = xf_font_dense();

    lv_style_init(&s_screen);
    lv_style_set_bg_color(&s_screen, s_colors.bg);
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_text_color(&s_screen, s_colors.text_primary);
    lv_style_set_text_font(&s_screen, body);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_radius(&s_screen, 0);

    lv_style_init(&s_body);
    lv_style_set_bg_opa(&s_body, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_body, 0);
    lv_style_set_radius(&s_body, 0);
    lv_style_set_pad_hor(&s_body, 12);
    lv_style_set_pad_top(&s_body, 4);
    lv_style_set_pad_bottom(&s_body, 8);
    lv_style_set_pad_row(&s_body, 10);
    /* Wide scrollbars would eat 12 px of a 480 px screen; the design keeps a
     * thin indicator so a long list still signals that more content exists. */
    lv_style_set_width(&s_body, LV_PCT(100));

    lv_style_init(&s_card);
    lv_style_set_bg_color(&s_card, s_colors.card);
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_radius(&s_card, 14);
    lv_style_set_border_width(&s_card, 0);
    lv_style_set_pad_all(&s_card, 10);
    lv_style_set_text_color(&s_card, s_colors.text_primary);
    lv_style_set_text_font(&s_card, dense);
    /* A card must not swallow vertical drags: the catalogue scrolls as a whole,
     * and a drag is never interpreted as a tap (design note 7). */
    lv_style_set_pad_row(&s_card, 4);

    lv_style_init(&s_card_selected);
    lv_style_set_border_width(&s_card_selected, 3);
    lv_style_set_border_color(&s_card_selected, s_colors.brand_green);
    lv_style_set_bg_color(&s_card_selected, s_colors.card_highlight);

    lv_style_init(&s_button_primary);
    lv_style_set_bg_color(&s_button_primary, s_colors.brand_green);
    lv_style_set_bg_opa(&s_button_primary, LV_OPA_COVER);
    lv_style_set_radius(&s_button_primary, 12);
    lv_style_set_border_width(&s_button_primary, 0);
    lv_style_set_text_color(&s_button_primary, s_colors.on_green);
    lv_style_set_text_font(&s_button_primary, body);
    lv_style_set_pad_hor(&s_button_primary, 14);

    lv_style_init(&s_button_secondary);
    lv_style_set_bg_color(&s_button_secondary, s_colors.brand_teal);
    lv_style_set_bg_opa(&s_button_secondary, LV_OPA_COVER);
    lv_style_set_radius(&s_button_secondary, 12);
    lv_style_set_border_width(&s_button_secondary, 0);
    lv_style_set_text_color(&s_button_secondary, s_colors.text_primary);
    lv_style_set_text_font(&s_button_secondary, body);
    lv_style_set_pad_hor(&s_button_secondary, 14);

    lv_style_init(&s_button_flat);
    lv_style_set_bg_opa(&s_button_flat, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_button_flat, 0);
    lv_style_set_radius(&s_button_flat, 10);
    lv_style_set_text_color(&s_button_flat, s_colors.text_primary);
    lv_style_set_text_font(&s_button_flat, dense);
    lv_style_set_pad_hor(&s_button_flat, 8);
    lv_style_set_pad_ver(&s_button_flat, 4);

    lv_style_init(&s_chip);
    lv_style_set_bg_color(&s_chip, s_colors.card);
    lv_style_set_bg_opa(&s_chip, LV_OPA_COVER);
    lv_style_set_radius(&s_chip, 16);
    lv_style_set_border_width(&s_chip, 0);
    lv_style_set_text_color(&s_chip, s_colors.text_secondary);
    lv_style_set_text_font(&s_chip, dense);
    lv_style_set_pad_hor(&s_chip, 12);
    lv_style_set_pad_ver(&s_chip, 6);

    lv_style_init(&s_chip_active);
    lv_style_set_bg_color(&s_chip_active, s_colors.brand_green);
    lv_style_set_text_color(&s_chip_active, s_colors.on_green);

    lv_style_init(&s_action_bar);
    lv_style_set_bg_color(&s_action_bar, s_colors.brand_teal);
    lv_style_set_bg_opa(&s_action_bar, LV_OPA_COVER);
    lv_style_set_radius(&s_action_bar, 0);
    lv_style_set_border_width(&s_action_bar, 0);
    lv_style_set_pad_hor(&s_action_bar, 16);
    lv_style_set_pad_ver(&s_action_bar, 10);

    lv_style_init(&s_divider);
    lv_style_set_bg_opa(&s_divider, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_divider, 0);
    lv_style_set_border_side(&s_divider, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&s_divider, s_colors.divider);
    lv_style_set_border_opa(&s_divider, LV_OPA_COVER);
    lv_style_set_radius(&s_divider, 0);

    lv_style_init(&s_progress_track);
    lv_style_set_bg_color(&s_progress_track, s_colors.divider);
    lv_style_set_bg_opa(&s_progress_track, LV_OPA_COVER);
    lv_style_set_radius(&s_progress_track, 0);
    lv_style_set_border_width(&s_progress_track, 0);

    lv_style_init(&s_progress_indicator);
    lv_style_set_bg_color(&s_progress_indicator, s_colors.brand_green);
    lv_style_set_bg_opa(&s_progress_indicator, LV_OPA_COVER);
    lv_style_set_radius(&s_progress_indicator, 0);
    lv_style_set_border_width(&s_progress_indicator, 0);
}

lv_style_t *xf_style_screen(void)
{
    return &s_screen;
}

lv_style_t *xf_style_body(void)
{
    return &s_body;
}

lv_style_t *xf_style_card(void)
{
    return &s_card;
}

lv_style_t *xf_style_card_selected(void)
{
    return &s_card_selected;
}

lv_style_t *xf_style_button_primary(void)
{
    return &s_button_primary;
}

lv_style_t *xf_style_button_secondary(void)
{
    return &s_button_secondary;
}

lv_style_t *xf_style_button_flat(void)
{
    return &s_button_flat;
}

lv_style_t *xf_style_chip(void)
{
    return &s_chip;
}

lv_style_t *xf_style_chip_active(void)
{
    return &s_chip_active;
}

lv_style_t *xf_style_action_bar(void)
{
    return &s_action_bar;
}

lv_style_t *xf_style_divider(void)
{
    return &s_divider;
}

lv_style_t *xf_style_progress_track(void)
{
    return &s_progress_track;
}

lv_style_t *xf_style_progress_indicator(void)
{
    return &s_progress_indicator;
}