/**
 * @file xf_product_ids.h
 * @brief Generated file. Do not edit.
 *
 * Stable product, asset and spec-field identifiers for the 18 fixed
 * XIAO models. Order equals the default carousel order (design note 4.1)
 * and must never be renumbered.
 *
 * Generator: tools/gen_xf_data.py
 * Source: product_catalog.json + asset_manifest.json
 * product_catalog.json sha256: 749204e621bba388ac7e5b26021ec258590416c21937efac94599627ea41ecd9
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Stable business key; the numeric value is also the carousel order. */
typedef enum {
    XF_PRODUCT_SAMD21 = 0,
    XF_PRODUCT_SAMD21_PLUS = 1,
    XF_PRODUCT_STM32C5 = 2,
    XF_PRODUCT_MG24_SENSE = 3,
    XF_PRODUCT_RP2350 = 4,
    XF_PRODUCT_RP2040 = 5,
    XF_PRODUCT_RP2040_PLUS = 6,
    XF_PRODUCT_RA4M1 = 7,
    XF_PRODUCT_ESP32_C3 = 8,
    XF_PRODUCT_ESP32_C5 = 9,
    XF_PRODUCT_ESP32_C6 = 10,
    XF_PRODUCT_ESP32_S3_PLUS = 11,
    XF_PRODUCT_ESP32_S3_SENSE = 12,
    XF_PRODUCT_NRF52840_SENSE = 13,
    XF_PRODUCT_NRF52840_PLUS = 14,
    XF_PRODUCT_NRF54L15_SENSE = 15,
    XF_PRODUCT_NRF54LM20A_SENSE = 16,
    XF_PRODUCT_NRF54LM20B_SENSE = 17,
    XF_PRODUCT_COUNT = 18,
    XF_PRODUCT_INVALID = 0xFF,
} xf_product_id_t;

/** Image asset key; one per product, replaceable without touching identity. */
typedef enum {
    XF_ASSET_SAMD21_FRONT = 0,
    XF_ASSET_SAMD21_PLUS_FRONT = 1,
    XF_ASSET_STM32C5_FRONT = 2,
    XF_ASSET_MG24_SENSE_FRONT = 3,
    XF_ASSET_RP2350_FRONT = 4,
    XF_ASSET_RP2040_FRONT = 5,
    XF_ASSET_RP2040_PLUS_FRONT = 6,
    XF_ASSET_RA4M1_FRONT = 7,
    XF_ASSET_ESP32_C3_FRONT = 8,
    XF_ASSET_ESP32_C5_FRONT = 9,
    XF_ASSET_ESP32_C6_FRONT = 10,
    XF_ASSET_ESP32_S3_PLUS_FRONT = 11,
    XF_ASSET_ESP32_S3_SENSE_FRONT = 12,
    XF_ASSET_NRF52840_SENSE_FRONT = 13,
    XF_ASSET_NRF52840_PLUS_FRONT = 14,
    XF_ASSET_NRF54L15_SENSE_FRONT = 15,
    XF_ASSET_NRF54LM20A_SENSE_FRONT = 16,
    XF_ASSET_NRF54LM20B_SENSE_FRONT = 17,
    XF_ASSET_COUNT = 18,
} xf_asset_id_t;

/** Spec rows rendered on DETAIL and COMPARE, in display order. */
typedef enum {
    XF_SPEC_FIELD_PROCESSOR = 0,
    XF_SPEC_FIELD_CPU_CLOCK_MAX = 1,
    XF_SPEC_FIELD_SRAM = 2,
    XF_SPEC_FIELD_INTERNAL_NVM = 3,
    XF_SPEC_FIELD_INTERNAL_NVM_TYPE = 4,
    XF_SPEC_FIELD_EXTERNAL_FLASH = 5,
    XF_SPEC_FIELD_PSRAM = 6,
    XF_SPEC_FIELD_NPU = 7,
    XF_SPEC_FIELD_PIO_STATE_MACHINES = 8,
    XF_SPEC_FIELD_WIRELESS = 9,
    XF_SPEC_FIELD_EXPOSED_GPIO = 10,
    XF_SPEC_FIELD_INTERFACES = 11,
    XF_SPEC_FIELD_ADC_RESOLUTION = 12,
    XF_SPEC_FIELD_DAC_RESOLUTION = 13,
    XF_SPEC_FIELD_SENSORS = 14,
    XF_SPEC_FIELD_ONBOARD_BATTERY_CHARGING = 15,
    XF_SPEC_FIELD_FOOTPRINT = 16,
    XF_SPEC_FIELD_APPLICATIONS = 17,
    XF_SPEC_FIELD_COUNT = 18,
} xf_spec_field_t;

/** Detail page groups (design note 6.4). */
typedef enum {
    XF_SECTION_COMPUTE = 0,
    XF_SECTION_CONNECT = 1,
    XF_SECTION_SENSORS = 2,
    XF_SECTION_POWER = 3,
    XF_SECTION_APPLICATIONS = 4,
    XF_SECTION_COUNT = 5,
} xf_spec_section_t;

/** COMPARE tabs (design note 6.5). */
typedef enum {
    XF_COMPARE_CATEGORY_COMPUTE = 0,
    XF_COMPARE_CATEGORY_CONNECT = 1,
    XF_COMPARE_CATEGORY_SENSORS = 2,
    XF_COMPARE_CATEGORY_COUNT = 3,
} xf_compare_category_t;

/** Number of distinct manufacturers referenced by the catalogue. */
#define XF_MANUFACTURER_COUNT 7

/** Maximum number of products in one comparison (design note 4.2). */
#define XF_COMPARE_CAPACITY 3

/** Catalog size as a compile-time constant. */
#define XF_PRODUCT_CATALOG_SIZE 18

#ifdef __cplusplus
}
#endif
