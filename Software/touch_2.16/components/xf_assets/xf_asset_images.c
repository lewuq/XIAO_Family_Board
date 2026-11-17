/**
 * @file xf_asset_images.c
 * @brief Flash-resident image table for the 18 XIAO models.
 *
 * The raw RGB565A8 payloads are produced offline by tools/convert_assets.py and
 * linked with target_add_binary_data(), so the UI draws directly from flash:
 * no decode step, no heap, no failure path at run time.
 *
 * One descriptor per asset key, one table indexed by xf_asset_id_t. The table
 * is written with designated initialisers so a future asset key can never shift
 * the meaning of an existing index.
 */

#include "xf_asset.h"

/** Only RGB565A8 is generated; a different LVGL colour depth would blit garbage. */
_Static_assert(LV_COLOR_DEPTH == 16, "xf_assets expects a 16 bpp LVGL build");

#define XF_EMBED(name)                                                      \
    extern const uint8_t name##_bin[] asm("_binary_" #name "_bin_start");   \
    extern const uint8_t name##_bin_end[] asm("_binary_" #name "_bin_end");

/**
 * @brief Declare one descriptor on top of an embedded binary.
 *
 * `stride` is the RGB565 plane stride (two bytes per pixel); the alpha plane
 * follows the whole colour plane, which is exactly what LVGL 9 expects for
 * LV_COLOR_FORMAT_RGB565A8.
 */
#define XF_IMAGE(name, width, height)                                        \
    XF_EMBED(name)                                                           \
    static const lv_image_dsc_t dsc_##name = {                               \
        .header = {                                                          \
            .magic = LV_IMAGE_HEADER_MAGIC,                                  \
            .cf = LV_COLOR_FORMAT_RGB565A8,                                  \
            .flags = 0,                                                      \
            .w = (width),                                                    \
            .h = (height),                                                   \
            .stride = (width) * 2,                                           \
            .reserved_2 = 0,                                                 \
        },                                                                   \
        .data_size = (width) * (height) * 3,                                 \
        .data = name##_bin,                                                  \
        .reserved = NULL,                                                    \
        .reserved_2 = NULL,                                                  \
    };

#define XF_IMAGE_PAIR(key)                                                   \
    XF_IMAGE(hero_##key, XF_ASSET_HERO_W, XF_ASSET_HERO_H)                    \
    XF_IMAGE(thumb_##key, XF_ASSET_THUMB_W, XF_ASSET_THUMB_H)                 \
    static const xf_asset_image_t pair_##key = {                             \
        .hero = &dsc_hero_##key,                                             \
        .thumb = &dsc_thumb_##key,                                           \
    };

/** One pair per entry of xf_asset_id_t, in catalogue order. */
XF_IMAGE_PAIR(samd21_front)
XF_IMAGE_PAIR(samd21_plus_front)
XF_IMAGE_PAIR(stm32c5_front)
XF_IMAGE_PAIR(mg24_sense_front)
XF_IMAGE_PAIR(rp2350_front)
XF_IMAGE_PAIR(rp2040_front)
XF_IMAGE_PAIR(rp2040_plus_front)
XF_IMAGE_PAIR(ra4m1_front)
XF_IMAGE_PAIR(esp32_c3_front)
XF_IMAGE_PAIR(esp32_c5_front)
XF_IMAGE_PAIR(esp32_c6_front)
XF_IMAGE_PAIR(esp32_s3_plus_front)
XF_IMAGE_PAIR(esp32_s3_sense_front)
XF_IMAGE_PAIR(nrf52840_sense_front)
XF_IMAGE_PAIR(nrf52840_plus_front)
XF_IMAGE_PAIR(nrf54l15_sense_front)
XF_IMAGE_PAIR(nrf54lm20a_sense_front)
XF_IMAGE_PAIR(nrf54lm20b_sense_front)

static const xf_asset_image_t *const g_asset_pairs[XF_ASSET_COUNT] = {
    [XF_ASSET_SAMD21_FRONT] = &pair_samd21_front,
    [XF_ASSET_SAMD21_PLUS_FRONT] = &pair_samd21_plus_front,
    [XF_ASSET_STM32C5_FRONT] = &pair_stm32c5_front,
    [XF_ASSET_MG24_SENSE_FRONT] = &pair_mg24_sense_front,
    [XF_ASSET_RP2350_FRONT] = &pair_rp2350_front,
    [XF_ASSET_RP2040_FRONT] = &pair_rp2040_front,
    [XF_ASSET_RP2040_PLUS_FRONT] = &pair_rp2040_plus_front,
    [XF_ASSET_RA4M1_FRONT] = &pair_ra4m1_front,
    [XF_ASSET_ESP32_C3_FRONT] = &pair_esp32_c3_front,
    [XF_ASSET_ESP32_C5_FRONT] = &pair_esp32_c5_front,
    [XF_ASSET_ESP32_C6_FRONT] = &pair_esp32_c6_front,
    [XF_ASSET_ESP32_S3_PLUS_FRONT] = &pair_esp32_s3_plus_front,
    [XF_ASSET_ESP32_S3_SENSE_FRONT] = &pair_esp32_s3_sense_front,
    [XF_ASSET_NRF52840_SENSE_FRONT] = &pair_nrf52840_sense_front,
    [XF_ASSET_NRF52840_PLUS_FRONT] = &pair_nrf52840_plus_front,
    [XF_ASSET_NRF54L15_SENSE_FRONT] = &pair_nrf54l15_sense_front,
    [XF_ASSET_NRF54LM20A_SENSE_FRONT] = &pair_nrf54lm20a_sense_front,
    [XF_ASSET_NRF54LM20B_SENSE_FRONT] = &pair_nrf54lm20b_sense_front,
};

const xf_asset_image_t *xf_asset_images(uint8_t asset_id)
{
    if (asset_id >= XF_ASSET_COUNT) {
        return NULL;
    }
    return g_asset_pairs[asset_id];
}

const lv_image_dsc_t *xf_asset_hero(uint8_t asset_id)
{
    const xf_asset_image_t *pair = xf_asset_images(asset_id);
    return pair != NULL ? pair->hero : NULL;
}

const lv_image_dsc_t *xf_asset_thumb(uint8_t asset_id)
{
    const xf_asset_image_t *pair = xf_asset_images(asset_id);
    return pair != NULL ? pair->thumb : NULL;
}

const lv_font_t *xf_font_title(void)
{
    return &xf_font_ui_32;
}

const lv_font_t *xf_font_body(void)
{
    return &xf_font_ui_22;
}

const lv_font_t *xf_font_dense(void)
{
    return &xf_font_ui_18;
}

/** Detail-only fonts: rounded 130% sizes; other pages keep their base sizes. */
const lv_font_t *xf_font_detail_body(void)
{
    return &xf_font_detail_29;
}

const lv_font_t *xf_font_detail_dense(void)
{
    return &xf_font_detail_23;
}
