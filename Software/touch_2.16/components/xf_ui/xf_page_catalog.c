/**
 * @file xf_page_catalog.c
 * @brief CATALOG and SELECT: one layout, two click semantics.
 *
 * Reusing the grid is a design decision, not a shortcut: the only difference the
 * user must perceive is "tap a card to read it" versus "tap a card to pick it",
 * so the difference is carried by the mode title, the checked badge and the
 * action bar, never by a different layout (design note 5).
 */

#include <stdint.h>
#include <stdio.h>

#include "xf_asset.h"
#include "xf_product.h"
#include "xf_strings.h"
#include "xf_theme.h"
#include "xf_ui_format.h"
#include "xf_ui_internal.h"
#include "xf_ui_widgets.h"

/** Two columns adapt to the inset content rectangle without horizontal clipping. */
#define CARD_W ((XF_UI_CONTENT_W - 16) / 2)
#define CARD_H 148
#define CARD_THUMB_W 80
#define CARD_THUMB_H 87
/** Filter strip occupies the band between the top bar and the grid. */
#define FILTER_TOP XF_UI_TOP_BAR_H
#define FILTER_H 64
#define GRID_TOP (FILTER_TOP + FILTER_H)

/** @brief Ordinal of a product inside the comparison, or -1 when absent. */
static int8_t selection_ordinal(const xf_app_state_t *state, uint8_t product_id)
{
    for (uint8_t i = 0; i < state->selected_count && i < XF_COMPARE_CAPACITY; i++) {
        if (state->selected_ids[i] == product_id) {
            return (int8_t)(i + 1U);
        }
    }
    return -1;
}

static void back_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_CLOSE_PAGE, XF_PRODUCT_INVALID, -1, 0);
}

static void filter_cb(lv_event_t *event)
{
    int16_t index = (int16_t)(intptr_t)lv_event_get_user_data(event);
    xf_ui_post(XF_EVENT_SET_FILTER, XF_PRODUCT_INVALID, index, 0);
}

static void card_cb(lv_event_t *event)
{
    uint8_t product_id = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    const xf_app_state_t *state = xf_ui_state();
    if (state->screen == (uint8_t)XF_SCREEN_SELECT) {
        xf_ui_post(XF_EVENT_TOGGLE_COMPARE, product_id, -1, 0);
    } else {
        xf_ui_post(XF_EVENT_OPEN_PRODUCT, product_id, -1, 0);
    }
}

static void select_mode_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_EDIT_SELECTION, XF_PRODUCT_INVALID, -1, 0);
}

static void compare_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_OPEN_COMPARE, XF_PRODUCT_INVALID, -1, 0);
}

static void clear_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_CLEAR_SELECTION, XF_PRODUCT_INVALID, -1, 0);
}

static void differences_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_TOGGLE_DIFFERENCES, XF_PRODUCT_INVALID, -1, 0);
}

static void grid_scroll_cb(lv_event_t *event)
{
    lv_obj_t *grid = lv_event_get_target(event);
    xf_ui_post_scroll(lv_obj_get_scroll_y(grid));
}

/** @brief Build one filter chip ("All" plus the seven manufacturers). */
static void build_chip(lv_obj_t *parent, const char *text, bool active, int16_t index)
{
    const xf_theme_colors_t *colors = xf_theme();
    lv_obj_t *chip = lv_button_create(parent);
    lv_obj_remove_style_all(chip);
    lv_obj_add_style(chip, xf_style_chip(), LV_PART_MAIN);
    if (active) {
        lv_obj_add_style(chip, xf_style_chip_active(), LV_PART_MAIN);
    }
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_min_width(chip, 64, 0);
    lv_obj_set_style_pad_hor(chip, 14, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(chip);
    lv_label_set_text(label, text != NULL ? text : "");
    /* An active chip is not distinguishable by colour alone, so its label is
     * set in the dark "on green" ink as well. */
    lv_obj_set_style_text_color(label, active ? colors->on_green : colors->text_secondary, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(chip, filter_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);
}

/** @brief Draw the checked badge: ordinal number plus a filled disc. */
static void build_selection_badge(lv_obj_t *card, int8_t ordinal)
{
    const xf_theme_colors_t *colors = xf_theme();
    lv_obj_t *badge = lv_obj_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 28, 28);
    lv_obj_set_style_radius(badge, 14, 0);
    lv_obj_set_style_bg_color(badge, colors->brand_green, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 2, -2);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    char text[4];
    text[0] = (char)('0' + (ordinal % 10));
    text[1] = '\0';
    lv_obj_t *label = lv_label_create(badge);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, colors->on_green, 0);
    lv_obj_set_style_text_font(label, xf_font_dense(), 0);
    lv_obj_center(label);
}

/** @brief Build one product card: thumbnail, model, manufacturer, badge. */
static void build_card(lv_obj_t *grid, const xf_product_t *product, bool select_mode,
                       int8_t ordinal)
{
    const xf_theme_colors_t *colors = xf_theme();

    lv_obj_t *card = lv_button_create(grid);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, xf_style_card(), LV_PART_MAIN);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_pad_column(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    /* Press feedback inside 80 ms: the fill changes on press, and the release
     * commits the action (design note 6.1). */
    lv_obj_set_style_bg_color(card, colors->card_highlight, LV_STATE_PRESSED);
    if (ordinal > 0) {
        lv_obj_add_style(card, xf_style_card_selected(), LV_PART_MAIN);
    }

    const lv_image_dsc_t *thumb = xf_asset_thumb(product->asset);
    if (thumb != NULL) {
        lv_obj_t *image = lv_image_create(card);
        lv_image_set_src(image, thumb);
        lv_obj_set_size(image, CARD_THUMB_W, CARD_THUMB_H);
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_t *missing = xf_widget_wrapped_label(card, xf_string(XF_STRING_ASSET_UNAVAILABLE),
                                                    xf_font_dense(), colors->warning, true, 96);
        lv_obj_set_height(missing, XF_ASSET_THUMB_H);
    }

    lv_obj_t *text = lv_obj_create(card);
    lv_obj_remove_style_all(text);
    lv_obj_set_size(text, CARD_W - 16 - CARD_THUMB_W - 8, CARD_H - 16);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(text, 0, 0);
    lv_obj_set_style_pad_row(text, 2, 0);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(text, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = xf_widget_wrapped_label(text, product->name, xf_font_dense(),
                                             colors->text_primary, true,
                                             CARD_W - 16 - CARD_THUMB_W - 8);
    lv_obj_set_style_max_height(name, 108, 0);

    lv_obj_t *manufacturer = xf_widget_label(text, xf_product_manufacturer(product),
                                             xf_font_dense(), colors->text_secondary, true);
    lv_obj_set_width(manufacturer,
                     CARD_W - 16 - CARD_THUMB_W - 8);

    if (select_mode && ordinal > 0) {
        build_selection_badge(card, ordinal);
    }

    lv_obj_add_event_cb(card, card_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)product->id);
}

xf_page_result_t xf_page_catalog_create(lv_obj_t *root, const xf_app_state_t *state,
                                        bool select_mode)
{
    xf_page_result_t result = {0};
    const xf_theme_colors_t *colors = xf_theme();

    xf_top_bar_t bar = xf_widget_top_bar(
        root, xf_string(select_mode ? XF_STRING_SCREEN_SELECT_TITLE : XF_STRING_SCREEN_CATALOG_TITLE),
        true, back_cb, NULL);

    char counter[8];
    (void)snprintf(counter, sizeof(counter), "%u/3", (unsigned)state->selected_count);
    const char *count_text = counter;
    if (bar.trailing != NULL && count_text != NULL) {
        lv_obj_t *count = xf_widget_label(bar.trailing, count_text, xf_font_dense(),
                                          colors->text_secondary, true);
        lv_obj_set_style_text_align(count, LV_TEXT_ALIGN_RIGHT, 0);
    }

    /* Filter strip: horizontal scrolling labels, never a dropdown, because a
     * dropdown would need an overlay and lose the "always visible" affordance. */
    lv_obj_t *filters = lv_obj_create(root);
    lv_obj_remove_style_all(filters);
    lv_obj_set_size(filters, XF_UI_SCREEN_W, FILTER_H);
    lv_obj_set_pos(filters, 0, FILTER_TOP);
    lv_obj_set_flex_flow(filters, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(filters, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(filters, 12, 0);
    lv_obj_set_style_pad_column(filters, 8, 0);
    lv_obj_set_style_bg_color(filters, colors->bg, 0);
    lv_obj_set_style_bg_opa(filters, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(filters, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(filters, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(filters, LV_OBJ_FLAG_SCROLLABLE);

    build_chip(filters, xf_string(XF_STRING_FILTER_ALL), state->manufacturer_filter < 0, -1);
    for (int16_t i = 0; i < (int16_t)XF_MANUFACTURER_COUNT; i++) {
        build_chip(filters, xf_manufacturer_names[i], state->manufacturer_filter == (int8_t)i, i);
    }

    int32_t notice_height = xf_page_attach_notice(root, state);
    int32_t grid_height = XF_UI_ACTION_BAR_Y - notice_height - GRID_TOP;

    lv_obj_t *grid = xf_widget_body(root, GRID_TOP, grid_height);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(grid, 12, 0);
    lv_obj_set_style_pad_ver(grid, 12, 0);
    lv_obj_set_style_pad_row(grid, 12, 0);
    lv_obj_set_style_pad_column(grid, 16, 0);
    lv_obj_add_event_cb(grid, grid_scroll_cb, LV_EVENT_SCROLL, NULL);

    for (uint8_t i = 0; i < XF_PRODUCT_COUNT; i++) {
        const xf_product_t *product = xf_product_find(i);
        if (product == NULL) {
            continue;
        }
        /* Filtering only changes what is visible; the selection set is untouched
         * and selected products hidden by the filter stay selected (design note 7). */
        if (state->manufacturer_filter >= 0 &&
            product->manufacturer_index != (uint8_t)state->manufacturer_filter) {
            continue;
        }
        build_card(grid, product, select_mode, selection_ordinal(state, product->id));
    }

    lv_obj_t *action = xf_widget_action_bar(root);

    if (select_mode) {
        xf_widget_flat_button(action, xf_string(XF_STRING_ACTION_CLEAR), clear_cb, NULL);

        lv_obj_t *differences = lv_button_create(action);
        lv_obj_remove_style_all(differences);
        lv_obj_add_style(differences, xf_style_chip(), LV_PART_MAIN);
        if (state->compare_differences_only) {
            lv_obj_add_style(differences, xf_style_chip_active(), LV_PART_MAIN);
        }
        lv_obj_set_size(differences, LV_SIZE_CONTENT, 44);
        lv_obj_clear_flag(differences, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *differences_label = lv_label_create(differences);
        lv_label_set_text(differences_label, xf_string(XF_STRING_COMPARE_DIFFERENCES_ONLY));
        lv_obj_set_style_text_font(differences_label, xf_font_dense(), 0);
        lv_obj_set_style_text_color(
            differences_label,
            state->compare_differences_only ? colors->on_green : colors->text_secondary, 0);
        lv_obj_center(differences_label);
        lv_obj_add_event_cb(differences, differences_cb, LV_EVENT_CLICKED, NULL);
    } else {
        /* The selection entry is the catalogue's primary action: a filled,
         * outlined touch target with explicit pressed feedback. */
        lv_obj_t *select = xf_widget_button(action, xf_string(XF_STRING_SCREEN_SELECT_TITLE),
                                            true, select_mode_cb, NULL);
        lv_obj_set_size(select, 200, 48);
        lv_obj_set_style_outline_color(select, colors->brand_green, 0);
        lv_obj_set_style_outline_width(select, 1, 0);
        lv_obj_set_style_outline_pad(select, 2, 0);
        lv_obj_set_style_bg_color(select, lv_color_darken(colors->brand_green, LV_OPA_20),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_translate_y(select, 1, LV_STATE_PRESSED);
    }

    /* The Compare button stays visible even with fewer than two products: it is
     * the affordance that explains the rule, and tapping it shows
     * "Select at least 2 products" instead of silently doing nothing. */
    xf_widget_button(action, xf_string(XF_STRING_ACTION_COMPARE), select_mode, compare_cb, NULL);

    result.scrollable = grid;
    return result;
}