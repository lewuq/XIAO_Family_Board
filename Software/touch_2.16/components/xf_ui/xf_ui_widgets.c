/**
 * @file xf_ui_widgets.c
 * @brief Widget builders shared by all pages.
 */

#include "xf_ui_widgets.h"

#include <stddef.h>

#include "xf_asset.h"
#include "xf_strings.h"
#include "xf_theme.h"
#include "xf_ui_format.h"
#include "xf_ui_internal.h"

/** @brief Apply a style list to an object without owning it. */
static void add_style(lv_obj_t *obj, lv_style_t *style)
{
    if (obj != NULL && style != NULL) {
        lv_obj_add_style(obj, style, LV_PART_MAIN);
    }
}

lv_obj_t *xf_widget_root(void)
{
    lv_obj_t *root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root);
    add_style(root, xf_style_screen());
    lv_obj_set_size(root, XF_UI_SCREEN_W, XF_UI_SCREEN_H);
    lv_obj_set_pos(root, XF_UI_SAFE_INSET, XF_UI_SAFE_INSET);
    lv_obj_set_style_pad_all(root, 0, 0);
    /* Absolute children: the design gives an explicit rectangle for every
     * region, so flex would only move those rectangles around. */
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    /* Clickable so that a press on empty background still reaches the input
     * layer; the carousel is one large "tap to explore" target (design note 6.2).
     */
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    return root;
}

static void language_cb(lv_event_t *event)
{
    (void)event;
    if (xf_ui_gesture_is_tap()) {
        xf_ui_post(XF_EVENT_TOGGLE_LANGUAGE, XF_PRODUCT_INVALID, -1, 0);
    }
}

lv_obj_t *xf_widget_language_button(lv_obj_t *parent)
{
    lv_obj_t *button = xf_widget_flat_button(parent, xf_language_button_text(), language_cb, NULL);
    lv_obj_set_style_text_font(button, xf_font_dense(), 0);
    lv_obj_set_width(button, 80);
    return button;
}

xf_top_bar_t xf_widget_top_bar(lv_obj_t *root, const char *title, bool with_back,
                               lv_event_cb_t back_cb, void *back_user)
{
    xf_top_bar_t bar = {0};
    const xf_theme_colors_t *colors = xf_theme();

    bar.bar = lv_obj_create(root);
    lv_obj_remove_style_all(bar.bar);
    lv_obj_set_size(bar.bar, XF_UI_SCREEN_W, XF_UI_TOP_BAR_H);
    lv_obj_set_pos(bar.bar, 0, 0);
    lv_obj_set_style_bg_color(bar.bar, colors->bg, 0);
    lv_obj_set_style_bg_opa(bar.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar.bar, 8, 0);
    lv_obj_set_style_pad_ver(bar.bar, 6, 0);
    lv_obj_set_flex_flow(bar.bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar.bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar.bar, 8, 0);
    lv_obj_clear_flag(bar.bar, LV_OBJ_FLAG_SCROLLABLE);

    if (with_back) {
        xf_widget_flat_button(bar.bar, xf_string(XF_STRING_NAV_BACK), back_cb, back_user);
    }

    bar.title = xf_widget_label(bar.bar, title, xf_font_body(), colors->text_primary, true);
    lv_obj_set_flex_grow(bar.title, 1);
    lv_label_set_long_mode(bar.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(bar.title, LV_TEXT_ALIGN_LEFT, 0);

    bar.trailing = lv_obj_create(bar.bar);
    lv_obj_remove_style_all(bar.trailing);
    lv_obj_set_size(bar.trailing, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar.trailing, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar.trailing, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar.trailing, 6, 0);
    lv_obj_clear_flag(bar.trailing, LV_OBJ_FLAG_SCROLLABLE);
    xf_widget_language_button(bar.trailing);
    return bar;
}

lv_obj_t *xf_widget_body(lv_obj_t *root, int32_t top, int32_t height)
{
    lv_obj_t *body = lv_obj_create(root);
    lv_obj_remove_style_all(body);
    add_style(body, xf_style_body());
    lv_obj_set_size(body, XF_UI_SCREEN_W, height);
    lv_obj_set_pos(body, 0, top);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    /* The design asks for a discoverable scroll indicator, not a fat bar that
     * eats a card column, so the bar is kept thin and pale. */
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLL_ELASTIC);
    return body;
}

lv_obj_t *xf_widget_action_bar(lv_obj_t *root)
{
    lv_obj_t *bar = lv_obj_create(root);
    lv_obj_remove_style_all(bar);
    add_style(bar, xf_style_action_bar());
    lv_obj_set_size(bar, XF_UI_SCREEN_W, XF_UI_BOTTOM_BAR_H);
    lv_obj_set_pos(bar, 0, XF_UI_ACTION_BAR_Y);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 10, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    return bar;
}

lv_obj_t *xf_widget_button(lv_obj_t *parent, const char *text, bool primary,
                           lv_event_cb_t cb, void *user)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    add_style(button, primary ? xf_style_button_primary() : xf_style_button_secondary());
    lv_obj_set_height(button, 44);
    lv_obj_set_width(button, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(button, 80, 0);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_max_width(label, 300, 0);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user);
    }
    return button;
}

lv_obj_t *xf_widget_flat_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                                void *user)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    add_style(button, xf_style_button_flat());
    lv_obj_set_size(button, LV_SIZE_CONTENT, 40);
    /* 80 px minimum: the design forbids tap targets narrower than that. */
    lv_obj_set_style_min_width(button, 80, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user);
    }
    return button;
}

lv_obj_t *xf_widget_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                          lv_color_t color, bool use_color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    if (use_color) {
        lv_obj_set_style_text_color(label, color, 0);
    }
    return label;
}

lv_obj_t *xf_widget_wrapped_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                                  lv_color_t color, bool use_color, int32_t width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(label, width);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    if (use_color) {
        lv_obj_set_style_text_color(label, color, 0);
    }
    return label;
}

lv_obj_t *xf_widget_logo(lv_obj_t *parent)
{
    const xf_theme_colors_t *colors = xf_theme();

    lv_obj_t *plate = lv_obj_create(parent);
    lv_obj_remove_style_all(plate);
    lv_obj_set_size(plate, 248, 36);
    lv_obj_set_style_bg_color(plate, colors->logo_bg, 0);
    lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(plate, 6, 0);
    lv_obj_set_style_pad_all(plate, 0, 0);
    lv_obj_clear_flag(plate, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *logo = lv_image_create(plate);
    lv_image_set_src(logo, &xf_brand_logo);
    lv_obj_center(logo);
    return plate;
}

lv_obj_t *xf_widget_divider(lv_obj_t *parent)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    add_style(line, xf_style_divider());
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

lv_obj_t *xf_widget_notice(lv_obj_t *parent, const char *text)
{
    if (text == NULL) {
        return NULL;
    }
    const xf_theme_colors_t *colors = xf_theme();
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, XF_UI_SCREEN_W, XF_UI_NOTICE_H);
    lv_obj_set_pos(line, 0, XF_UI_NOTICE_Y);
    lv_obj_set_style_bg_color(line, colors->bg, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(line, 16, 0);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = xf_widget_label(line, text, xf_font_dense(), colors->warning, true);
    lv_obj_set_width(label, XF_UI_SCREEN_W - 32);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return line;
}

int32_t xf_page_attach_notice(lv_obj_t *root, const xf_app_state_t *state)
{
    if (root == NULL || state == NULL) {
        return 0;
    }
    char text[XF_UI_VALUE_MAX];
    const char *label = xf_notice_text(state->notice, state->selected_count, text, sizeof(text));
    if (label == NULL) {
        return 0;
    }
    /* The strip is part of the layout budget: the page shrinks its body by the
     * returned height so the action bar never covers the last row. */
    xf_widget_notice(root, label);
    return XF_UI_NOTICE_H;
}