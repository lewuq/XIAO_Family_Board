/**
 * @file xf_page_attract.c
 * @brief Carousel and manual navigation inside a rounded-square safe area.
 *
 * Background/hero taps open details. Footer buttons have independent click
 * handlers; the input-device release event never navigates on their behalf.
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

typedef struct {
    lv_obj_t *manufacturer, *counter, *image, *name, *feature;
    lv_obj_t *dots[3];
    uint8_t product_id, language;
} xf_home_view_t;
static xf_home_view_t s_home;

static void open_product_cb(lv_event_t *event)
{
    /* Children such as footer buttons must not also trigger background action. */
    if (lv_event_get_target(event) != lv_event_get_current_target(event) ||
        !xf_ui_gesture_is_tap()) {
        return;
    }
    /* Cached identity is the product actually painted, even before queued
     * state changes have been committed. */
    uint8_t id = s_home.product_id;
    xf_ui_post(XF_EVENT_OPEN_PRODUCT, id, -1, 0);
}

static void browse_cb(lv_event_t *event)
{
    if (!xf_ui_gesture_is_tap()) {
        return;
    }
    (void)event;
    xf_ui_post(XF_EVENT_OPEN_CATALOG, XF_PRODUCT_INVALID, -1, 0);
}

static void build_top_bar(lv_obj_t *root, const xf_product_t *product,
                          uint8_t product_id)
{
    const xf_theme_colors_t *colors = xf_theme();
    lv_obj_t *plate = xf_widget_logo(root);
    if (plate != NULL) {
        lv_obj_set_pos(plate, 12, 6);
        lv_obj_clear_flag(plate, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *manufacturer = xf_widget_label(root, xf_product_manufacturer(product),
                                             xf_font_dense(), colors->text_secondary, true);
    lv_obj_set_pos(manufacturer, 12, 44);
    lv_obj_set_width(manufacturer, XF_UI_CONTENT_W);
    lv_obj_set_style_text_align(manufacturer, LV_TEXT_ALIGN_CENTER, 0);
    s_home.manufacturer = manufacturer;

    char counter[12];
    (void)snprintf(counter, sizeof(counter), "%02u/%u", (unsigned)(product_id + 1U),
                   (unsigned)XF_PRODUCT_COUNT);
    lv_obj_t *index = xf_widget_label(root, counter, xf_font_dense(), colors->brand_green, true);
    lv_obj_set_pos(index, 266, 14);
    lv_obj_set_width(index, 68);
    lv_obj_set_style_text_align(index, LV_TEXT_ALIGN_RIGHT, 0);
    s_home.counter = index;
    lv_obj_t *language = xf_widget_language_button(root);
    lv_obj_set_pos(language, 340, 4);
    lv_obj_set_size(language, 80, 40);
}

static void build_hero(lv_obj_t *root, const xf_product_t *product)
{
    const lv_image_dsc_t *hero = xf_asset_hero(product->asset);
    if (hero == NULL) {
        lv_obj_t *missing = xf_widget_wrapped_label(
            root, xf_string(XF_STRING_ASSET_UNAVAILABLE), xf_font_body(),
            xf_theme()->warning, true, XF_UI_CONTENT_W);
        lv_obj_set_pos(missing, 12, 160);
        return;
    }
    lv_obj_t *image = lv_image_create(root);
    lv_image_set_src(image, hero);
    s_home.image = image;
    /* Contain preserves the source aspect ratio inside the safe image box. */
    lv_obj_set_size(image, 208, 210);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_pos(image, (XF_UI_SCREEN_W - 208) / 2, 72);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
}

/** Three rolling indicator positions represent the complete 18-product carousel. */
static void build_dots(lv_obj_t *root, uint8_t product_id)
{
    for (uint8_t dot = 0; dot < 3U; ++dot) {
        bool active = dot == product_id % 3U;
        lv_obj_t *circle = lv_obj_create(root);
        s_home.dots[dot] = circle;
        lv_obj_remove_style_all(circle);
        lv_obj_set_size(circle, 8, 8);
        lv_obj_set_pos(circle, XF_UI_SCREEN_W / 2 - 24 + dot * 20, 294);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circle, active ? xf_theme()->brand_green
                                                 : xf_theme()->text_secondary, 0);
        lv_obj_set_style_bg_opa(circle, active ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    }
}

xf_page_result_t xf_page_attract_create(lv_obj_t *root, const xf_app_state_t *state)
{
    xf_page_result_t result = {0};
    const xf_theme_colors_t *colors = xf_theme();
    s_home = (xf_home_view_t){0};
    s_home.language = state->language;
    uint8_t product_id = xf_app_visible_product(state);
    const xf_product_t *product = xf_product_find(product_id);
    if (product == NULL) {
        return result;
    }

    s_home.product_id = product_id;
    lv_obj_add_event_cb(root, open_product_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)product_id);
    build_top_bar(root, product, product_id);
    build_dots(root, product_id);
    build_hero(root, product);

    lv_obj_t *name = xf_widget_wrapped_label(root, product->name, xf_font_body(),
                                             colors->text_primary, true, XF_UI_CONTENT_W);
    lv_obj_set_pos(name, 12, 310);
    lv_obj_set_style_max_height(name, 44, 0);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    s_home.name = name;

    lv_obj_t *feature = xf_widget_label(root, xf_translate_text(product->feature), xf_font_dense(),
                                         colors->text_secondary, true);
    lv_obj_set_pos(feature, 12, 356);
    lv_obj_set_width(feature, XF_UI_CONTENT_W);
    lv_obj_set_style_text_align(feature, LV_TEXT_ALIGN_CENTER, 0);
    s_home.feature = feature;
    lv_obj_t *browse = xf_widget_button(root, xf_string(XF_STRING_ACTION_BROWSE), true,
                                        browse_cb, NULL);
    lv_obj_set_size(browse, 200, 44);
    lv_obj_set_pos(browse, (XF_UI_SCREEN_W - 200) / 2, 384);
    return result;
}

/** Repaint changed content while retaining the logo, controls and object tree. */
bool xf_page_attract_update(const xf_app_state_t *state)
{
    if (s_home.image == NULL || state->language != s_home.language) {
        return false;
    }
    const xf_product_t *product = xf_product_find(xf_app_visible_product(state));
    const lv_image_dsc_t *image = product != NULL ? xf_asset_hero(product->asset) : NULL;
    if (product == NULL || image == NULL) {
        return false;
    }
    if (product->id == s_home.product_id) {
        return true;
    }
    char counter[12];
    (void)snprintf(counter, sizeof(counter), "%02u/%u", (unsigned)(product->id + 1U),
                   (unsigned)XF_PRODUCT_COUNT);
    lv_label_set_text(s_home.counter, counter);
    lv_label_set_text(s_home.manufacturer, xf_product_manufacturer(product));
    lv_label_set_text(s_home.name, product->name);
    lv_label_set_text(s_home.feature, xf_translate_text(product->feature));
    lv_image_set_src(s_home.image, image);
    for (uint8_t n = 0; n < 3U; ++n) {
        bool active = n == product->id % 3U;
        lv_obj_set_style_bg_color(s_home.dots[n], active ? xf_theme()->brand_green
                                                       : xf_theme()->text_secondary, 0);
        lv_obj_set_style_bg_opa(s_home.dots[n], active ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    s_home.product_id = product->id;
    return true;
}
