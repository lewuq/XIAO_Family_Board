/**
 * @file xf_product_lookup.c
 * @brief Bounds-checked access to the generated catalogue tables.
 *
 * Pure C, no LVGL and no hardware. Every accessor validates its index before
 * touching the tables so a corrupted identifier can never read past the end;
 * the caller still owns the decision of what to render for a missing value.
 */

#include <string.h>

#include "xf_product.h"

const xf_product_t *xf_product_find(uint8_t id)
{
    if (id >= XF_PRODUCT_COUNT) {
        return NULL;
    }
    return &xf_products[id];
}

const xf_product_t *xf_product_find_by_key(const char *id_string)
{
    if (id_string == NULL) {
        return NULL;
    }
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; id++) {
        if (strcmp(xf_product_id_strings[id], id_string) == 0) {
            return &xf_products[id];
        }
    }
    return NULL;
}

const xf_spec_t *xf_product_spec(uint8_t id, uint8_t field)
{
    if (id >= XF_PRODUCT_COUNT || field >= XF_SPEC_FIELD_COUNT) {
        return NULL;
    }
    return &xf_product_specs[id][field];
}

const char *xf_product_manufacturer(const xf_product_t *product)
{
    if (product == NULL || product->manufacturer_index >= XF_MANUFACTURER_COUNT) {
        return "";
    }
    return xf_manufacturer_names[product->manufacturer_index];
}

uint8_t xf_product_next(uint8_t id)
{
    if (id >= XF_PRODUCT_COUNT) {
        return 0;
    }
    return (uint8_t)((id + 1U) % XF_PRODUCT_COUNT);
}

uint8_t xf_product_prev(uint8_t id)
{
    if (id >= XF_PRODUCT_COUNT) {
        return (uint8_t)(XF_PRODUCT_COUNT - 1U);
    }
    return (uint8_t)((id + XF_PRODUCT_COUNT - 1U) % XF_PRODUCT_COUNT);
}

bool xf_spec_field_in_category(uint8_t field, uint8_t category)
{
    if (field >= XF_SPEC_FIELD_COUNT || category >= XF_COMPARE_CATEGORY_COUNT) {
        return false;
    }
    return xf_spec_field_compare_category[field] == (int8_t)category;
}

uint8_t xf_spec_category_field_count(uint8_t category)
{
    if (category >= XF_COMPARE_CATEGORY_COUNT) {
        return 0;
    }
    uint8_t count = 0;
    for (uint8_t field = 0; field < XF_SPEC_FIELD_COUNT; field++) {
        if (xf_spec_field_compare_category[field] == (int8_t)category) {
            count++;
        }
    }
    return count;
}

bool xf_spec_category_field_at(uint8_t category, uint8_t ordinal, uint8_t *field)
{
    if (category >= XF_COMPARE_CATEGORY_COUNT || field == NULL) {
        return false;
    }
    uint8_t index = 0;
    for (uint8_t candidate = 0; candidate < XF_SPEC_FIELD_COUNT; candidate++) {
        if (xf_spec_field_compare_category[candidate] != (int8_t)category) {
            continue;
        }
        if (index == ordinal) {
            *field = candidate;
            return true;
        }
        index++;
    }
    return false;
}