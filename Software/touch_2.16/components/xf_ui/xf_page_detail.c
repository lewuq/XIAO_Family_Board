/**
 * @file xf_page_detail.c
 * @brief DETAIL: everything known about one model, grouped the way the design
 *        groups it, with every gap shown as a gap.
 *
 * The page never invents a value: a pending or conflicting source renders
 * "Not verified", an absent feature renders "None", a non-applicable one "N/A"
 * (design note 6.4). The placeholder image is labelled "Reference image" so a
 * stand-in can never be mistaken for a photograph of the shipped board.
 */

#include <stdint.h>

#include "xf_asset.h"
#include "xf_product.h"
#include "xf_strings.h"
#include "xf_theme.h"
#include "xf_ui_format.h"
#include "xf_ui_internal.h"
#include "xf_ui_widgets.h"

/** Aspect-preserving hero box inside the product header. */
#define DETAIL_IMAGE_W 160
/** Field name column width, value column is the rest of the XF_UI_CONTENT_W px content box. */
#define DETAIL_LABEL_W 164
#define DETAIL_VALUE_W (XF_UI_CONTENT_W - DETAIL_LABEL_W - 8)

/* Handles describe the current detail tree only; business state stays in core. */
static lv_obj_t *s_detail_root;
static lv_obj_t *s_detail_action;
static lv_obj_t *s_detail_notice;
static lv_obj_t *s_detail_notice_label;
static uint8_t s_detail_product;
static uint8_t s_detail_language;
static bool s_detail_selected;
static bool s_detail_compare_ready;

static void back_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_CLOSE_PAGE, XF_PRODUCT_INVALID, -1, 0);
}

static void catalog_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_OPEN_CATALOG, XF_PRODUCT_INVALID, -1, 0);
}

static void toggle_compare_cb(lv_event_t *event)
{
    uint8_t product_id = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    xf_ui_post(XF_EVENT_TOGGLE_COMPARE, product_id, -1, 0);
}

static void compare_cb(lv_event_t *event)
{
    (void)event;
    xf_ui_post(XF_EVENT_OPEN_COMPARE, XF_PRODUCT_INVALID, -1, 0);
}

static void body_scroll_cb(lv_event_t *event)
{
    lv_obj_t *body = lv_event_get_target(event);
    xf_ui_post_scroll(lv_obj_get_scroll_y(body));
}

/** @brief Transparent row container that neither scrolls nor captures clicks. */
static lv_obj_t *make_row(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/** @brief One "name + value" row, with the amber ink reserved for gaps. */
static void build_field_row(lv_obj_t *parent, const xf_product_t *product, uint8_t field)
{
    const xf_theme_colors_t *colors = xf_theme();
    const xf_spec_t *spec = xf_product_spec(product->id, field);
    if (spec == NULL) {
        return;
    }

    char value[XF_UI_VALUE_MAX];
    const char *text = xf_format_spec(spec, value, sizeof(value), false);
    bool verified = spec->status == (uint8_t)XF_SPEC_VERIFIED &&
                    spec->kind != (uint8_t)XF_SPEC_KIND_EMPTY;

    lv_obj_t *row = make_row(parent, XF_UI_CONTENT_W);
    lv_obj_t *name = xf_widget_wrapped_label(row, xf_field_label(field), xf_font_detail_dense(),
                                             colors->text_secondary, true, DETAIL_LABEL_W);
    lv_obj_t *value_label = xf_widget_wrapped_label(
        row, text, xf_font_detail_dense(), verified ? colors->text_primary : colors->warning, true,
        DETAIL_VALUE_W);
    (void)name;
    (void)value_label;
    xf_widget_divider(parent);
}

/** @brief Draw the hero image and the identity block beside it. */
static void build_header(lv_obj_t *parent, const xf_product_t *product)
{
    const xf_theme_colors_t *colors = xf_theme();
    lv_obj_t *head = make_row(parent, XF_UI_CONTENT_W);
    lv_obj_set_style_pad_ver(head, 0, 0);

    const lv_image_dsc_t *hero = xf_asset_hero(product->asset);
    if (hero != NULL) {
        lv_obj_t *image = lv_image_create(head);
        lv_image_set_src(image, hero);
        lv_obj_set_size(image, DETAIL_IMAGE_W, 174);
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_t *missing = xf_widget_wrapped_label(head, xf_string(XF_STRING_ASSET_UNAVAILABLE),
                                                    xf_font_detail_dense(), colors->warning, true,
                                                    DETAIL_IMAGE_W);
        lv_obj_set_height(missing, 174);
    }

    lv_obj_t *info = lv_obj_create(head);
    lv_obj_remove_style_all(info);
    lv_obj_set_size(info, XF_UI_CONTENT_W - DETAIL_IMAGE_W - 8, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(info, 6, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);

    xf_widget_wrapped_label(info, product->name, xf_font_detail_body(), colors->text_primary, true,
                            XF_UI_CONTENT_W - DETAIL_IMAGE_W - 8);
    xf_widget_wrapped_label(info, xf_product_manufacturer(product), xf_font_detail_dense(),
                            colors->text_secondary, true, XF_UI_CONTENT_W - DETAIL_IMAGE_W - 8);
    xf_widget_wrapped_label(info, xf_translate_text(product->feature), xf_font_detail_dense(), colors->text_secondary,
                            true, XF_UI_CONTENT_W - DETAIL_IMAGE_W - 8);

    /* The stand-in rule is a visible caption, not a hidden attribute. */
    if (product->is_placeholder) {
        xf_widget_wrapped_label(info, xf_string(XF_STRING_ASSET_REFERENCE_IMAGE),
                                xf_font_detail_dense(), colors->warning, true,
                                XF_UI_CONTENT_W - DETAIL_IMAGE_W - 8);
    }
}

/** @brief Draw every field of one section, in catalogue order. */
static void build_section(lv_obj_t *parent, const xf_product_t *product, uint8_t section)
{
    const xf_theme_colors_t *colors = xf_theme();

    lv_obj_t *title = xf_widget_label(parent, xf_section_label(section), xf_font_detail_body(),
                                      colors->brand_green, true);
    lv_obj_set_width(title, XF_UI_CONTENT_W);
    lv_obj_set_style_pad_top(title, 8, 0);

    for (uint8_t field = 0; field < (uint8_t)XF_SPEC_FIELD_COUNT; field++) {
        if (xf_spec_field_section[field] != section) {
            continue;
        }
        if (section == (uint8_t)XF_SECTION_APPLICATIONS) {
            /* The section header already says "Applications", so the list is
             * rendered as a paragraph instead of a repeated name/value row. */
            const xf_spec_t *spec = xf_product_spec(product->id, field);
            char value[XF_UI_VALUE_MAX];
            const char *text = xf_format_spec(spec, value, sizeof(value), false);
            xf_widget_wrapped_label(parent, text, xf_font_detail_dense(), colors->text_primary, true, XF_UI_CONTENT_W);
            continue;
        }
        build_field_row(parent, product, field);
    }
}

/** Apply detail typography to buttons whose shared style supplies a base font. */
static void detail_buttons(lv_obj_t *parent, const lv_font_t *font)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        lv_obj_set_style_text_font(child, font, 0);
    }
}

static void detail_actions(const xf_app_state_t *state)
{
    lv_obj_clean(s_detail_action);
    bool selected = xf_compare_contains(state, s_detail_product);
    xf_widget_button(s_detail_action,
                     xf_string(selected ? XF_STRING_ACTION_REMOVE_COMPARE : XF_STRING_ACTION_ADD_COMPARE),
                     !selected, toggle_compare_cb, (void *)(uintptr_t)s_detail_product);
    if (xf_compare_ready(state)) {
        xf_widget_button(s_detail_action, xf_string(XF_STRING_ACTION_COMPARE), true, compare_cb, NULL);
    }
    detail_buttons(s_detail_action, xf_font_detail_body());
    s_detail_selected = selected;
    s_detail_compare_ready = xf_compare_ready(state);
}

static void detail_notice(const xf_app_state_t *state)
{
    char text[XF_UI_VALUE_MAX];
    const char *message = xf_notice_text(state->notice, state->selected_count, text, sizeof(text));
    if (message == NULL) {
        lv_obj_add_flag(s_detail_notice, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_detail_notice_label, message);
        lv_obj_remove_flag(s_detail_notice, LV_OBJ_FLAG_HIDDEN);
    }
}

bool xf_page_detail_update(const xf_app_state_t *state)
{
    if (s_detail_root == NULL || state->current_product_id != s_detail_product ||
        state->language != s_detail_language) {
        return false;
    }
    /* Reserve the notice band, so expiry never changes body height, forces a
     * scroll clamp, or rebuilds every specification row. */
    detail_notice(state);
    if (xf_compare_contains(state, s_detail_product) != s_detail_selected ||
        xf_compare_ready(state) != s_detail_compare_ready) {
        detail_actions(state);
    }
    return true;
}

xf_page_result_t xf_page_detail_create(lv_obj_t *root, const xf_app_state_t *state)
{
    xf_page_result_t result = {0};
    const xf_theme_colors_t *colors = xf_theme();

    uint8_t product_id = xf_app_visible_product(state);
    const xf_product_t *product = xf_product_find(product_id);
    if (product == NULL) {
        xf_widget_label(root, xf_string(XF_STRING_DATA_UNAVAILABLE), xf_font_detail_body(),
                        colors->warning, true);
        return result;
    }

    xf_top_bar_t bar = xf_widget_top_bar(root, xf_string(XF_STRING_SCREEN_DETAIL_TITLE), true,
                                        back_cb, NULL);
    if (bar.trailing != NULL) {
        xf_widget_flat_button(bar.trailing, xf_string(XF_STRING_ACTION_BROWSE), catalog_cb, NULL);
        detail_buttons(bar.trailing, xf_font_detail_dense());
    }

    detail_buttons(bar.bar, xf_font_detail_dense());
    lv_obj_set_style_text_font(bar.title, xf_font_detail_body(), 0);
    s_detail_root = root;
    s_detail_product = product_id;
    s_detail_language = state->language;
    s_detail_notice = xf_widget_notice(root, "");
    s_detail_notice_label = lv_obj_get_child(s_detail_notice, 0);
    lv_obj_set_style_text_font(s_detail_notice_label, xf_font_detail_dense(), 0);
    detail_notice(state);
    /* Stable geometry during selection/notice updates keeps inertial scrolling. */
    int32_t notice_height = XF_UI_NOTICE_H;
    lv_obj_t *body = xf_widget_body(root, XF_UI_TOP_BAR_H,
                                    XF_UI_ACTION_BAR_Y - notice_height - XF_UI_TOP_BAR_H);
    lv_obj_add_event_cb(body, body_scroll_cb, LV_EVENT_SCROLL, NULL);

    build_header(body, product);
    for (uint8_t section = 0; section < (uint8_t)XF_SECTION_COUNT; section++) {
        build_section(body, product, section);
    }

    /* Runtime notes are shown as a plain closing line: they describe data gaps
     * and must never be presented as a feature of the product. */
    if (product->notes != NULL && product->notes[0] != '\0') {
        xf_widget_wrapped_label(body, xf_translate_text(product->notes), xf_font_detail_dense(), colors->text_secondary,
                                true, XF_UI_CONTENT_W);
    }

    s_detail_action = xf_widget_action_bar(root);
    detail_actions(state);

    result.scrollable = body;
    return result;
}