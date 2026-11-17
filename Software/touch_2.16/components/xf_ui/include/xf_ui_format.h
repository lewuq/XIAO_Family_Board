/**
 * @file xf_ui_format.h
 * @brief Turning catalogue values and message keys into visible text.
 *
 * One place decides how a missing value looks, so "Not verified" can never
 * appear on one screen while another screen quietly shows 0 MHz (design note
 * 6.4 and acceptance case "missing parameter"). The formatters write into a
 * caller-provided buffer: no allocation, no static state, so they are safe to
 * call from the controller task and from host tests.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xf_product.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Recommended buffer size for one rendered spec value. */
#define XF_UI_VALUE_MAX 160

/**
 * @brief Render one spec cell.
 * @param[in] spec Cell to render; NULL yields the "data unavailable" text.
 * @param[out] buf Destination, always NUL terminated on return.
 * @param[in] len Capacity of @p buf in bytes; 0 leaves the buffer untouched.
 * @param[in] multiline true to place the unit on its own line (COMPARE columns),
 *            false to keep value and unit on one line (DETAIL rows).
 * @return @p buf, for convenient use inside a function call.
 * @note PENDING and CONFLICT always render "Not verified": a conflicting source
 *       is not a value (design note 9.1).
 * @note SoC-scoped values get the "(SoC feature)" suffix so a silicon
 *       capability is never read as a board feature.
 */
const char *xf_format_spec(const xf_spec_t *spec, char *buf, size_t len, bool multiline);

/**
 * @brief Replace a single `{token}` placeholder such as `{count}`.
 * @param[in] template_text Message text from the string table.
 * @param[in] token Placeholder name without braces, e.g. "count".
 * @param[in] value Replacement text.
 * @param[out] buf Destination, always NUL terminated when @p len > 0.
 * @param[in] len Capacity of @p buf.
 * @return @p buf.
 * @note When the placeholder is absent, the template is copied unchanged. When
 *       the result would not fit, it is truncated at a byte boundary and never
 *       leaves an unterminated buffer.
 */
const char *xf_format_placeholder(const char *template_text, const char *token,
                                  const char *value, char *buf, size_t len);

/**
 * @brief Render the notice strip text for a notice kind.
 * @param[in] notice xf_notice_t value.
 * @param[in] count Selection size, used by the "{count} selected" texts.
 * @param[out] buf Destination, always NUL terminated when @p len > 0.
 * @param[in] len Capacity of @p buf.
 * @return @p buf, or NULL when the notice must not be shown.
 */
const char *xf_notice_text(uint8_t notice, uint8_t count, char *buf, size_t len);

/**
 * @brief Translate a spec field identifier through the string table.
 * @param[in] field xf_spec_field_t value.
 * @return Borrowed label, never NULL; falls back to "Data unavailable".
 */
const char *xf_field_label(uint8_t field);

/**
 * @brief Translate a DETAIL section identifier.
 * @param[in] section xf_spec_section_t value.
 * @return Borrowed label, never NULL.
 */
const char *xf_section_label(uint8_t section);

/**
 * @brief Translate a COMPARE tab identifier.
 * @param[in] category xf_compare_category_t value.
 * @return Borrowed label, never NULL.
 */
const char *xf_compare_category_label(uint8_t category);

#ifdef __cplusplus
}
#endif