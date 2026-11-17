/**
 * @file xf_compare.c
 * @brief Comparison set rules: capacity, ordering, removal and difference test.
 *
 * Pure logic, no LVGL and no hardware. The set is stored inline in the state so
 * the order of the three columns always equals the order of selection, which is
 * what the numbering 1/2/3 on the catalog cards shows.
 */

#include <string.h>

#include "xf_app_state.h"

static int32_t find_index(const xf_app_state_t *state, uint8_t product_id)
{
    for (uint8_t index = 0; index < state->selected_count; index++) {
        if (state->selected_ids[index] == product_id) {
            return (int32_t)index;
        }
    }
    return -1;
}

bool xf_compare_contains(const xf_app_state_t *state, uint8_t product_id)
{
    if (state == NULL) {
        return false;
    }
    return find_index(state, product_id) >= 0;
}

xf_result_t xf_compare_add(xf_app_state_t *state, uint8_t product_id)
{
    if (state == NULL) {
        return XF_ERR_INVALID_ARG;
    }
    if (xf_product_find(product_id) == NULL) {
        return XF_ERR_INVALID_ID;
    }
    if (xf_compare_contains(state, product_id)) {
        return XF_ERR_DUPLICATE;
    }
    /* Capacity is checked before any write so a fourth product cannot evict an
     * existing column (design note 7, "添加第 4 款"). */
    if (state->selected_count >= XF_COMPARE_CAPACITY) {
        return XF_ERR_COMPARE_FULL;
    }
    state->selected_ids[state->selected_count] = product_id;
    state->selected_count++;
    return XF_OK;
}

xf_result_t xf_compare_remove(xf_app_state_t *state, uint8_t product_id)
{
    if (state == NULL) {
        return XF_ERR_INVALID_ARG;
    }
    int32_t index = find_index(state, product_id);
    if (index < 0) {
        return XF_ERR_NOT_PRESENT;
    }
    for (uint8_t next = (uint8_t)(index + 1); next < state->selected_count; next++) {
        state->selected_ids[next - 1] = state->selected_ids[next];
    }
    state->selected_count--;
    state->selected_ids[state->selected_count] = 0;
    return XF_OK;
}

xf_result_t xf_compare_toggle(xf_app_state_t *state, uint8_t product_id)
{
    if (state == NULL) {
        return XF_ERR_INVALID_ARG;
    }
    if (xf_compare_contains(state, product_id)) {
        return xf_compare_remove(state, product_id);
    }
    return xf_compare_add(state, product_id);
}

void xf_compare_clear(xf_app_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state->selected_ids, 0, sizeof(state->selected_ids));
    state->selected_count = 0;
}

bool xf_compare_ready(const xf_app_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    return state->selected_count >= 2 && state->selected_count <= XF_COMPARE_CAPACITY;
}

uint8_t xf_compare_product_at(const xf_app_state_t *state, uint8_t ordinal)
{
    if (state == NULL || ordinal >= state->selected_count) {
        return XF_PRODUCT_INVALID;
    }
    return state->selected_ids[ordinal];
}

/**
 * @brief Compare two cells for the "Differences only" filter.
 *
 * A row counts as identical only when both cells are verified and carry the
 * same payload. Pending, conflicting and not-applicable cells never compare
 * equal: hiding them would silently drop a data gap from the table
 * (design note 9.2).
 */
static bool cells_identical(const xf_spec_t *left, const xf_spec_t *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }
    if (left->status != right->status) {
        return false;
    }
    if (left->status != XF_SPEC_VERIFIED) {
        return false;
    }
    if (left->kind != right->kind) {
        return false;
    }
    if (left->boolean != right->boolean) {
        return false;
    }
    const char *left_text = left->text ? left->text : "";
    const char *right_text = right->text ? right->text : "";
    if (strcmp(left_text, right_text) != 0) {
        return false;
    }
    const char *left_unit = left->unit ? left->unit : "";
    const char *right_unit = right->unit ? right->unit : "";
    return strcmp(left_unit, right_unit) == 0;
}

bool xf_compare_row_is_identical(const xf_app_state_t *state, uint8_t field)
{
    if (state == NULL || field >= XF_SPEC_FIELD_COUNT) {
        return false;
    }
    if (!xf_compare_ready(state)) {
        return false;
    }
    const xf_spec_t *reference = xf_product_spec(state->selected_ids[0], field);
    for (uint8_t index = 1; index < state->selected_count; index++) {
        if (!cells_identical(reference, xf_product_spec(state->selected_ids[index], field))) {
            return false;
        }
    }
    return true;
}