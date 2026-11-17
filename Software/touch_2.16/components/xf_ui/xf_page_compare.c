/**
 * @file xf_page_compare.c
 * @brief COMPARE: two or three models, one row height, one scroll position.
 *
 * The table is built as rows, not as separate columns: every row owns the field
 * name and the value cells, so all cells of a row always share the height and
 * the vertical scroll offset (design note 6.5). Nothing is ranked, scored or
 * inferred; the page only aligns what the catalogue already states.
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

/** Table geometry: XF_UI_CONTENT_W px content, 96 px field column, the rest split evenly. */
#define COMPARE_FIELD_W 96
#define COMPARE_HEADER_Y XF_UI_TOP_BAR_H
#define COMPARE_HEADER_H 144
#define COMPARE_TABS_Y (COMPARE_HEADER_Y + COMPARE_HEADER_H)
#define COMPARE_TABS_H 64
#define COMPARE_TABLE_Y (COMPARE_TABS_Y + COMPARE_TABS_H)
#define COMPARE_COLUMNS_MAX 3

static void back_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_CLOSE_PAGE, XF_PRODUCT_INVALID, -1, 0);
}

static void edit_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_EDIT_SELECTION, XF_PRODUCT_INVALID, -1, 0);
}

static void header_cb(lv_event_t *event)
{
    uint8_t product_id = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    xf_ui_post(XF_EVENT_OPEN_PRODUCT, product_id, -1, 0);
}

static void tab_cb(lv_event_t *event)
{
    int16_t category = (int16_t)(intptr_t)lv_event_get_user_data(event);
    xf_ui_post(XF_EVENT_SET_COMPARE_CATEGORY, XF_PRODUCT_INVALID, category, 0);
}

static void table_scroll_cb(lv_event_t *event)
{
    lv_obj_t *table = lv_event_get_target(event);
    xf_ui_post_scroll(lv_obj_get_scroll_y(table));
}

/** @brief Width of one product column for the current number of columns. */
static int32_t column_width(uint8_t count)
{
    if (count == 0U || count > COMPARE_COLUMNS_MAX) {
        return 0;
    }
    return (XF_UI_CONTENT_W - COMPARE_FIELD_W) / (int32_t)count;
}

/** @brief Row container that never captures clicks and never scrolls itself. */
static lv_obj_t *make_row(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/** @brief Build the fixed header: one tappable column per selected product. */
static void build_header(lv_obj_t *root, const xf_app_state_t *state)
{
    const xf_theme_colors_t *colors = xf_theme();
    uint8_t count = state->selected_count;
    int32_t width = column_width(count);

    lv_obj_t *head = lv_obj_create(root);
    lv_obj_remove_style_all(head);
    lv_obj_set_size(head, XF_UI_SCREEN_W, COMPARE_HEADER_H);
    lv_obj_set_pos(head, 0, COMPARE_HEADER_Y);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(head, 12, 0);
    lv_obj_set_style_bg_color(head, colors->bg, 0);
    lv_obj_set_style_bg_opa(head, LV_OPA_COVER, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    /* Spacer that keeps the product columns above their value columns. */
    lv_obj_t *spacer = lv_obj_create(head);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, COMPARE_FIELD_W, 1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < count && i < COMPARE_COLUMNS_MAX; i++) {
        uint8_t product_id = xf_compare_product_at(state, i);
        const xf_product_t *product = xf_product_find(product_id);
        if (product == NULL) {
            continue;
        }

        lv_obj_t *column = lv_button_create(head);
        lv_obj_remove_style_all(column);
        lv_obj_set_size(column, width, COMPARE_HEADER_H - 8);
        lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(column, 2, 0);
        lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);

        const lv_image_dsc_t *thumb = xf_asset_thumb(product->asset);
        if (thumb != NULL) {
            lv_obj_t *image = lv_image_create(column);
            lv_image_set_src(image, thumb);
            lv_obj_set_size(image, 68, 74);
            lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
            lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
        }

        lv_obj_t *name = xf_widget_wrapped_label(column, product->short_name, xf_font_dense(),
                                                 colors->text_primary, true, width - 4);
        /* At most three lines: a longer name is clipped rather than pushing the
         * table down and breaking the shared row grid. */
        lv_obj_set_style_max_height(name, 63, 0);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_add_event_cb(column, header_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)product_id);
    }
}

/** @brief Build the three category tabs. */
static void build_tabs(lv_obj_t *root, const xf_app_state_t *state)
{
    const xf_theme_colors_t *colors = xf_theme();

    lv_obj_t *tabs = lv_obj_create(root);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_size(tabs, XF_UI_SCREEN_W, COMPARE_TABS_H);
    lv_obj_set_pos(tabs, 0, COMPARE_TABS_Y);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(tabs, 12, 0);
    lv_obj_set_style_pad_column(tabs, 8, 0);
    lv_obj_set_style_bg_color(tabs, colors->bg, 0);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);

    for (int16_t category = 0; category < (int16_t)XF_COMPARE_CATEGORY_COUNT; category++) {
        bool active = state->compare_category == (uint8_t)category;

        lv_obj_t *tab = lv_button_create(tabs);
        lv_obj_remove_style_all(tab);
        lv_obj_add_style(tab, xf_style_chip(), LV_PART_MAIN);
        if (active) {
            lv_obj_add_style(tab, xf_style_chip_active(), LV_PART_MAIN);
        }
        lv_obj_set_size(tab, (XF_UI_CONTENT_W - 16) / 3, 44);
        lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *label = lv_label_create(tab);
        lv_label_set_text(label, xf_compare_category_label((uint8_t)category));
        lv_obj_set_style_text_font(label, xf_font_body(), 0);
        lv_obj_set_style_text_color(label, active ? colors->on_green : colors->text_secondary, 0);
        lv_obj_center(label);

        lv_obj_add_event_cb(tab, tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)category);
    }
}

/** @brief Build one table row: field name plus one cell per product. */
static void build_table_row(lv_obj_t *table, const xf_app_state_t *state, uint8_t field)
{
    const xf_theme_colors_t *colors = xf_theme();
    uint8_t count = state->selected_count;
    int32_t width = column_width(count);

    lv_obj_t *row = make_row(table, COMPARE_FIELD_W + (int32_t)count * width);

    lv_obj_t *name = xf_widget_wrapped_label(row, xf_field_label(field), xf_font_dense(),
                                             colors->text_secondary, true, COMPARE_FIELD_W - 4);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, 0);

    for (uint8_t i = 0; i < count && i < COMPARE_COLUMNS_MAX; i++) {
        uint8_t product_id = xf_compare_product_at(state, i);
        const xf_spec_t *spec = xf_product_spec(product_id, field);
        char value[XF_UI_VALUE_MAX];
        /* Units are allowed to break onto a second line so 240 MHz never has to
         * shrink below the 18 px minimum (design note 6.5). */
        const char *text = xf_format_spec(spec, value, sizeof(value), true);
        bool verified = spec != NULL && spec->status == (uint8_t)XF_SPEC_VERIFIED &&
                        spec->kind != (uint8_t)XF_SPEC_KIND_EMPTY;

        lv_obj_t *cell = xf_widget_wrapped_label(row, text, xf_font_dense(),
                                                 verified ? colors->text_primary : colors->warning,
                                                 true, width - 6);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    }
}

xf_page_result_t xf_page_compare_create(lv_obj_t *root, const xf_app_state_t *state)
{
    xf_page_result_t result = {0};
    const xf_theme_colors_t *colors = xf_theme();

    if (!xf_compare_ready(state)) {
        /* The reducer prevents this; the guard keeps the page honest if a future
         * caller reaches it anyway: never render a single-product comparison. */
        xf_widget_label(root, xf_string(XF_STRING_SELECTION_MINIMUM), xf_font_body(),
                        colors->warning, true);
        return result;
    }

    xf_top_bar_t bar =
        xf_widget_top_bar(root, xf_string(XF_STRING_SCREEN_COMPARE_TITLE), true, back_cb, NULL);
    if (bar.trailing != NULL) {
        char counter[8];
        (void)snprintf(counter, sizeof(counter), "%u/3", (unsigned)state->selected_count);
        const char *count_text = counter;
        xf_widget_label(bar.trailing, count_text, xf_font_dense(), colors->text_secondary, true);
        xf_widget_flat_button(bar.trailing, xf_string(XF_STRING_ACTION_EDIT), edit_cb, NULL);
    }

    build_header(root, state);
    build_tabs(root, state);

    lv_obj_t *table = lv_obj_create(root);
    lv_obj_remove_style_all(table);
    lv_obj_set_size(table, XF_UI_SCREEN_W, XF_UI_SCREEN_H - COMPARE_TABLE_Y);
    lv_obj_set_pos(table, 0, COMPARE_TABLE_Y);
    lv_obj_set_flex_flow(table, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(table, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(table, 12, 0);
    lv_obj_set_style_pad_ver(table, 6, 0);
    lv_obj_set_scroll_dir(table, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(table, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_flag(table, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(table, table_scroll_cb, LV_EVENT_SCROLL, NULL);

    uint8_t visible_rows = 0;
    for (uint8_t ordinal = 0; ordinal < (uint8_t)XF_SPEC_FIELD_COUNT; ordinal++) {
        uint8_t field = 0;
        if (!xf_spec_category_field_at(state->compare_category, ordinal, &field)) {
            break;
        }
        if (state->compare_differences_only && xf_compare_row_is_identical(state, field)) {
            continue;
        }
        build_table_row(table, state, field);
        xf_widget_divider(table);
        visible_rows++;
    }

    if (visible_rows == 0U) {
        /* Every row matched: say so explicitly instead of showing an empty pane
         * that looks like a failed load. */
        xf_widget_wrapped_label(table, xf_string(XF_STRING_COMPARE_DIFFERENCES_ONLY),
                                xf_font_dense(), colors->text_secondary, true, XF_UI_CONTENT_W);
    }

    result.scrollable = table;
    return result;
}