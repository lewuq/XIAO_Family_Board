/**
 * @file xf_content_status.h
 * @brief Status and scope tags attached to every catalogue spec value.
 *
 * The UI must render the three "no usable value" states differently
 * (design note 6.4):
 *   - XF_SPEC_PENDING / XF_SPEC_CONFLICT -> "Not verified"
 *   - XF_SPEC_NOT_APPLICABLE             -> "N/A"
 *   - a verified empty collection        -> "None"
 * Mixing them up would present a missing value as a real feature, so the
 * distinction is part of the data contract rather than a display choice.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Source verification state of one spec value (design note 9.1). */
typedef enum {
    XF_SPEC_VERIFIED = 0,       /**< Value confirmed by the cited source. */
    XF_SPEC_PENDING = 1,        /**< Not confirmed yet: render "Not verified". */
    XF_SPEC_CONFLICT = 2,       /**< Sources disagree: render "Not verified". */
    XF_SPEC_NOT_APPLICABLE = 3, /**< Feature absent by design: render "N/A". */
} xf_spec_status_t;

/** Ownership of the value: board level or silicon capability only. */
typedef enum {
    XF_SCOPE_BOARD = 0, /**< Value measured/implemented on the XIAO board. */
    XF_SCOPE_SOC = 1,   /**< Value describes the SoC, not the board. */
} xf_spec_scope_t;

/** Payload shape of one spec value. */
typedef enum {
    XF_SPEC_KIND_EMPTY = 0,  /**< No value; `status` decides the rendered text. */
    XF_SPEC_KIND_TEXT = 1,   /**< `text`, newline separated when it is a list. */
    XF_SPEC_KIND_NUMBER = 2, /**< `text` holds a normalised decimal number. */
    XF_SPEC_KIND_BOOL = 3,   /**< `boolean` holds yes/no. */
    XF_SPEC_KIND_NONE = 4,   /**< Verified as empty: render "None". */
} xf_spec_kind_t;

#ifdef __cplusplus
}
#endif