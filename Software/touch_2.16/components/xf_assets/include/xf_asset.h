/**
 * @file xf_asset.h
 * @brief Image, brand and font resources owned by xf_assets.
 *
 * Everything here is `static const` inside the flash image: no allocation, no
 * decoder, no file system access at run time. The UI only asks for a handle and
 * draws it, so a missing resource can never crash the render path
 * (design note 10.2).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "xf_fonts.h"
#include "xf_product_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hero image size in pixels. The two aspect ratios are fixed by
 * tools/convert_assets.py so ATTRIBUTES stay stable when the SVG is replaced.
 */
#define XF_ASSET_HERO_W 240
#define XF_ASSET_HERO_H 260
#define XF_ASSET_THUMB_W 96
#define XF_ASSET_THUMB_H 104

/** @brief Hero (240x260) and thumbnail (96x104) pair of one product. */
typedef struct {
    const lv_image_dsc_t *hero;  /**< Large front view, RGB565A8. */
    const lv_image_dsc_t *thumb; /**< List thumbnail, RGB565A8. */
} xf_asset_image_t;

/** @brief Official Seeed Studio logo, 240 x 27 px, drawn on #F5F8F7. */
extern const lv_image_dsc_t xf_brand_logo;

/** @brief SHA-256 of the source SVG the logo was rasterised from (AC-18). */
extern const char *const xf_brand_logo_source_sha256;

/**
 * @brief Fetch the image pair of an asset key.
 * @param[in] asset_id xf_asset_id_t value.
 * @return Borrowed pair, or NULL when the identifier is out of range.
 * @note Never NULL for a valid identifier; a missing file cannot happen because
 *       the images are linked into the firmware.
 */
const xf_asset_image_t *xf_asset_images(uint8_t asset_id);

/**
 * @brief Fetch only the hero image.
 * @param[in] asset_id xf_asset_id_t value.
 * @return Borrowed descriptor, or NULL when the identifier is out of range.
 */
const lv_image_dsc_t *xf_asset_hero(uint8_t asset_id);

/**
 * @brief Fetch only the thumbnail image.
 * @param[in] asset_id xf_asset_id_t value.
 * @return Borrowed descriptor, or NULL when the identifier is out of range.
 */
const lv_image_dsc_t *xf_asset_thumb(uint8_t asset_id);

/**
 * @brief Title font (32 px, 4 bpp).
 * @return Borrowed font, never NULL.
 */
const lv_font_t *xf_font_title(void);

/**
 * @brief Body font (22 px, 4 bpp).
 * @return Borrowed font, never NULL.
 */
const lv_font_t *xf_font_body(void);

/**
 * @brief Dense font (18 px, 4 bpp) used where 18 px is the minimum legible size.
 * @return Borrowed font, never NULL.
 */
const lv_font_t *xf_font_dense(void);

/** Detail-page fonts: 22 -> 29 px and 18 -> 23 px (approximately +30%). */
const lv_font_t *xf_font_detail_body(void);
const lv_font_t *xf_font_detail_dense(void);

#ifdef __cplusplus
}
#endif