/**
 * @file xf_ui_format.c
 * @brief Value and label formatting for DETAIL and COMPARE.
 */

#include "xf_ui_format.h"

#include <stdio.h>
#include <string.h>

#include "xf_app_state.h"
#include "xf_strings.h"

/** @brief Copy a borrowed string, tolerating NULL and truncation. */
static void copy_text(char *dst, size_t len, const char *src)
{
    if (dst == NULL || len == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i + 1U < len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/** @brief Append a borrowed string with a separator, tolerating truncation. */
static void append_text(char *dst, size_t len, const char *sep, const char *src)
{
    if (dst == NULL || len == 0U || src == NULL) {
        return;
    }
    size_t used = strlen(dst);
    if (used + 1U >= len) {
        return;
    }
    if (sep != NULL) {
        size_t i = 0;
        while (sep[i] != '\0' && used + 1U < len) {
            dst[used++] = sep[i++];
        }
    }
    size_t i = 0;
    while (src[i] != '\0' && used + 1U < len) {
        dst[used++] = src[i++];
    }
    dst[used] = '\0';
}

const char *xf_format_spec(const xf_spec_t *spec, char *buf, size_t len, bool multiline)
{
    if (buf == NULL || len == 0U) {
        return "";
    }
    buf[0] = '\0';

    if (spec == NULL) {
        copy_text(buf, len, xf_string(XF_STRING_DATA_UNAVAILABLE));
        return buf;
    }

    /* A value without a confirmed source is not a value: the two "unknown"
     * statuses both render the same words so the user cannot mistake one of
     * them for measured data (design note 9.1). */
    if (spec->status == (uint8_t)XF_SPEC_PENDING || spec->status == (uint8_t)XF_SPEC_CONFLICT) {
        copy_text(buf, len, xf_string(XF_STRING_VALUE_PENDING));
        return buf;
    }
    if (spec->status == (uint8_t)XF_SPEC_NOT_APPLICABLE) {
        copy_text(buf, len, xf_string(XF_STRING_VALUE_NOT_APPLICABLE));
        return buf;
    }

    switch ((xf_spec_kind_t)spec->kind) {
    case XF_SPEC_KIND_NONE:
        /* Verified as an empty collection, e.g. a board without sensors. */
        copy_text(buf, len, xf_string(XF_STRING_VALUE_NONE));
        return buf;
    case XF_SPEC_KIND_BOOL:
        copy_text(buf, len, xf_string(spec->boolean ? XF_STRING_VALUE_TRUE : XF_STRING_VALUE_FALSE));
        break;
    case XF_SPEC_KIND_NUMBER:
        copy_text(buf, len, spec->text);
        if (spec->unit != NULL) {
            append_text(buf, len, multiline ? "\n" : " ", spec->unit);
        }
        break;
    case XF_SPEC_KIND_TEXT:
    case XF_SPEC_KIND_EMPTY:
    default:
        if (spec->text == NULL || spec->text[0] == '\0') {
            /* Empty text without a status tag is still a gap, never a zero. */
            copy_text(buf, len, xf_string(XF_STRING_VALUE_PENDING));
            return buf;
        }
        copy_text(buf, len, xf_translate_text(spec->text));
        if (spec->unit != NULL) {
            append_text(buf, len, multiline ? "\n" : " ", spec->unit);
        }
        break;
    }

    if (spec->scope == (uint8_t)XF_SCOPE_SOC) {
        append_text(buf, len, multiline ? "\n" : " ", xf_string(XF_STRING_VALUE_SOC_SCOPE));
    }
    return buf;
}

const char *xf_format_placeholder(const char *template_text, const char *token,
                                  const char *value, char *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return "";
    }
    buf[0] = '\0';
    if (template_text == NULL) {
        return buf;
    }
    if (token == NULL || value == NULL) {
        copy_text(buf, len, template_text);
        return buf;
    }

    /* Build the search pattern "{token}" without allocating. */
    char pattern[24];
    if (strlen(token) + 3U >= sizeof(pattern)) {
        copy_text(buf, len, template_text);
        return buf;
    }
    pattern[0] = '{';
    size_t i = 0;
    while (token[i] != '\0') {
        pattern[i + 1U] = token[i];
        i++;
    }
    pattern[i + 1U] = '}';
    pattern[i + 2U] = '\0';

    const char *hit = strstr(template_text, pattern);
    if (hit == NULL) {
        copy_text(buf, len, template_text);
        return buf;
    }
    size_t prefix = (size_t)(hit - template_text);
    size_t used = 0;
    for (size_t p = 0; p < prefix && used + 1U < len; p++) {
        buf[used++] = template_text[p];
    }
    for (size_t v = 0; value[v] != '\0' && used + 1U < len; v++) {
        buf[used++] = value[v];
    }
    const char *tail = hit + strlen(pattern);
    for (size_t t = 0; tail[t] != '\0' && used + 1U < len; t++) {
        buf[used++] = tail[t];
    }
    buf[used] = '\0';
    return buf;
}

const char *xf_notice_text(uint8_t notice, uint8_t count, char *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return NULL;
    }
    buf[0] = '\0';
    char number[8];

    switch ((xf_notice_t)notice) {
    case XF_NOTICE_SELECTION_MIN:
        copy_text(buf, len, xf_string(XF_STRING_SELECTION_MINIMUM));
        return buf;
    case XF_NOTICE_SELECTION_MAX:
        copy_text(buf, len, xf_string(XF_STRING_SELECTION_MAXIMUM));
        return buf;
    case XF_NOTICE_SELECTION_EMPTY:
        copy_text(buf, len, xf_string(XF_STRING_SELECTION_EMPTY));
        return buf;
    case XF_NOTICE_SELECTION_READY:
        (void)snprintf(number, sizeof(number), "%u", (unsigned)count);
        return xf_format_placeholder(xf_string(XF_STRING_SELECTION_READY), "count", number,
                                     buf, len);
    case XF_NOTICE_IMAGE_UNAVAILABLE:
        copy_text(buf, len, xf_string(XF_STRING_ASSET_UNAVAILABLE));
        return buf;
    case XF_NOTICE_DATA_UNAVAILABLE:
        copy_text(buf, len, xf_string(XF_STRING_DATA_UNAVAILABLE));
        return buf;
    case XF_NOTICE_NONE:
    default:
        return NULL;
    }
}

const char *xf_field_label(uint8_t field)
{
    if (field >= (uint8_t)XF_SPEC_FIELD_COUNT) {
        return xf_string(XF_STRING_DATA_UNAVAILABLE);
    }
    const char *text = xf_string_lookup(xf_spec_field_msg_keys[field]);
    return text != NULL ? text : xf_string(XF_STRING_DATA_UNAVAILABLE);
}

const char *xf_section_label(uint8_t section)
{
    if (section >= (uint8_t)XF_SECTION_COUNT) {
        return xf_string(XF_STRING_DATA_UNAVAILABLE);
    }
    const char *text = xf_string_lookup(xf_section_msg_keys[section]);
    return text != NULL ? text : xf_string(XF_STRING_DATA_UNAVAILABLE);
}

const char *xf_compare_category_label(uint8_t category)
{
    if (category >= (uint8_t)XF_COMPARE_CATEGORY_COUNT) {
        return xf_string(XF_STRING_DATA_UNAVAILABLE);
    }
    const char *text = xf_string_lookup(xf_compare_category_msg_keys[category]);
    return text != NULL ? text : xf_string(XF_STRING_DATA_UNAVAILABLE);
}