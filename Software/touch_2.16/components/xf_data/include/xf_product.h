/**
 * @file xf_product.h
 * @brief Read-only product catalogue: stable identifiers, fields and queries.
 *
 * xf_data owns the single compiled copy of the catalogue. It never includes
 * LVGL, never touches hardware and never mutates state, so it is safe to read
 * from any task. All returned pointers are borrowed and live as long as the
 * application (static const storage).
 *
 * Values come from product_catalog.json through tools/gen_xf_data.py;
 * visible labels are message keys resolved by xf_ui, which keeps every visible
 * string in ui_strings.en.json (code rules 8.1).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xf_content_status.h"
#include "xf_product_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One catalogue cell: value plus its verification metadata. */
typedef struct {
    uint8_t status;  /**< xf_spec_status_t */
    uint8_t scope;   /**< xf_spec_scope_t */
    uint8_t kind;    /**< xf_spec_kind_t */
    const char *text;/**< Borrowed UTF-8 text; may contain '\n' for lists. */
    const char *unit;/**< Borrowed unit suffix, e.g. "MHz"; NULL when unitless. */
    bool boolean;    /**< Meaningful only for XF_SPEC_KIND_BOOL. */
} xf_spec_t;

/** One XIAO model. Identity is `id`; nothing else may be used as a key. */
typedef struct {
    uint8_t id;                         /**< xf_product_id_t */
    const char *id_string;              /**< Stable ASCII key, e.g. "samd21". */
    const char *name;                   /**< Full display name. */
    const char *short_name;             /**< COMPARE header label. */
    uint8_t manufacturer_index;         /**< Index into xf_manufacturer_names. */
    const char *feature;                /**< One-line highlight (feature_en). */
    const char *applications;           /**< '\n' separated application list. */
    const char *notes;                  /**< Runtime note / data gap (notes_en). */
    uint8_t asset;                      /**< xf_asset_id_t for the hero image. */
    bool is_placeholder;                /**< True when the image is a stand-in. */
    uint8_t placeholder_product_id;     /**< Source of the stand-in image. */
} xf_product_t;

/** Immutable catalogue tables (generated). */
extern const xf_product_t xf_products[XF_PRODUCT_COUNT];
extern const xf_spec_t xf_product_specs[XF_PRODUCT_COUNT][XF_SPEC_FIELD_COUNT];
extern const char *const xf_product_id_strings[XF_PRODUCT_COUNT];
extern const char *const xf_asset_key_strings[XF_ASSET_COUNT];
extern const char *const xf_manufacturer_names[XF_MANUFACTURER_COUNT];
extern const char *const xf_spec_field_msg_keys[XF_SPEC_FIELD_COUNT];
extern const uint8_t xf_spec_field_section[XF_SPEC_FIELD_COUNT];
extern const char *const xf_section_msg_keys[XF_SECTION_COUNT];
extern const char *const xf_compare_category_msg_keys[XF_COMPARE_CATEGORY_COUNT];
extern const int8_t xf_spec_field_compare_category[XF_SPEC_FIELD_COUNT];

/**
 * @brief Look up a product by stable identifier.
 * @param[in] id Identifier in [0, XF_PRODUCT_COUNT).
 * @return Borrowed product, or NULL for an unknown identifier.
 * @note Read-only; safe from any task. The result must not be freed.
 */
const xf_product_t *xf_product_find(uint8_t id);

/**
 * @brief Look up a product by its stable ASCII key.
 * @param[in] id_string Key such as "esp32_s3_plus"; NULL is not valid.
 * @return Borrowed product, or NULL when the key is unknown.
 */
const xf_product_t *xf_product_find_by_key(const char *id_string);

/**
 * @brief Read one spec cell.
 * @param[in] id Product identifier; out-of-range ids return NULL.
 * @param[in] field xf_spec_field_t value.
 * @return Borrowed cell, or NULL when either argument is out of range.
 */
const xf_spec_t *xf_product_spec(uint8_t id, uint8_t field);

/**
 * @brief Resolve the manufacturer display name of a product.
 * @param[in] product Borrowed product; NULL yields an empty string.
 * @return Borrowed display name, never NULL.
 */
const char *xf_product_manufacturer(const xf_product_t *product);

/**
 * @brief Next product in carousel order.
 * @param[in] id Current identifier; XF_PRODUCT_INVALID yields the first entry.
 * @return Identifier of the following product (wraps at the end).
 */
uint8_t xf_product_next(uint8_t id);

/**
 * @brief Previous product in carousel order.
 * @param[in] id Current identifier; XF_PRODUCT_INVALID yields the last entry.
 * @return Identifier of the preceding product (wraps at the start).
 */
uint8_t xf_product_prev(uint8_t id);

/**
 * @brief Test whether a field belongs to a COMPARE tab.
 * @param[in] field xf_spec_field_t value.
 * @param[in] category xf_compare_category_t value.
 * @return true when the field is listed under that category.
 */
bool xf_spec_field_in_category(uint8_t field, uint8_t category);

/**
 * @brief Count spec fields assigned to one COMPARE tab.
 * @param[in] category xf_compare_category_t value.
 * @return Number of fields, 0 for an out-of-range category.
 */
uint8_t xf_spec_category_field_count(uint8_t category);

/**
 * @brief Fetch the Nth field of one COMPARE tab.
 * @param[in] category xf_compare_category_t value.
 * @param[in] ordinal Index below xf_spec_category_field_count().
 * @param[out] field Receives the field identifier when found.
 * @return true when the ordinal exists.
 */
bool xf_spec_category_field_at(uint8_t category, uint8_t ordinal, uint8_t *field);

#ifdef __cplusplus
}
#endif