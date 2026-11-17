/*
 * 产品图片资源访问接口。
 *
 * 图片在构建时由 tools/convert_assets.py 转为 RGB565A8 裸数据，并通过
 * EMBED_FILES 嵌入固件（见 main/CMakeLists.txt），运行期不做解码、不占堆内存。
 */
#pragma once

#include "lvgl.h"
#include "product_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 画布尺寸必须与交接包 assets/hero|thumb 输出一致（设计说明 §5.1）。 */
#define PRODUCT_IMAGE_HERO_W 240
#define PRODUCT_IMAGE_HERO_H 260
#define PRODUCT_IMAGE_THUMB_W 96
#define PRODUCT_IMAGE_THUMB_H 104

#define PRODUCT_IMAGE_HERO_BYTES (PRODUCT_IMAGE_HERO_W * PRODUCT_IMAGE_HERO_H * 3)
#define PRODUCT_IMAGE_THUMB_BYTES (PRODUCT_IMAGE_THUMB_W * PRODUCT_IMAGE_THUMB_H * 3)

extern const lv_image_dsc_t *const g_product_hero[ASSET_COUNT];
extern const lv_image_dsc_t *const g_product_thumb[ASSET_COUNT];

static inline const lv_image_dsc_t *product_hero_image(uint16_t product_id)
{
    return (product_id < PRODUCT_COUNT) ? g_product_hero[product_id] : NULL;
}

static inline const lv_image_dsc_t *product_thumb_image(uint16_t product_id)
{
    return (product_id < PRODUCT_COUNT) ? g_product_thumb[product_id] : NULL;
}

#ifdef __cplusplus
}
#endif