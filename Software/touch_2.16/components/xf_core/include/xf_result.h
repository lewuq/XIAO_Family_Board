/**
 * @file xf_result.h
 * @brief Business result codes shared by xf_core, xf_ui and main.
 *
 * Business code never returns a meaningless ESP_FAIL: each value states what
 * the caller did wrong so the controller can show a precise notice and the log
 * keeps the original cause (code rules 7).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Outcome of a business operation. */
typedef enum {
    XF_OK = 0,                   /**< Event accepted, state updated. */
    XF_ERR_INVALID_ID = 1,       /**< Product identifier outside the catalogue. */
    XF_ERR_COMPARE_FULL = 2,     /**< Three products already selected. */
    XF_ERR_COMPARE_TOO_FEW = 3,  /**< Fewer than two products selected. */
    XF_ERR_DUPLICATE = 4,        /**< Identifier already present in the set. */
    XF_ERR_NOT_PRESENT = 5,      /**< Identifier not present in the set. */
    XF_ERR_INVALID_STATE = 6,    /**< Event not allowed on the current screen. */
    XF_ERR_INVALID_ARG = 7,      /**< Null pointer or out-of-range argument. */
    XF_ERR_UNAVAILABLE = 8,      /**< Required content or resource is missing. */
} xf_result_t;

/**
 * @brief Short stable name of a result, for logs only.
 * @param[in] result Result value.
 * @return Borrowed constant string, never NULL.
 */
static inline const char *xf_result_name(xf_result_t result)
{
    switch (result) {
    case XF_OK:
        return "ok";
    case XF_ERR_INVALID_ID:
        return "invalid_id";
    case XF_ERR_COMPARE_FULL:
        return "compare_full";
    case XF_ERR_COMPARE_TOO_FEW:
        return "compare_too_few";
    case XF_ERR_DUPLICATE:
        return "duplicate";
    case XF_ERR_NOT_PRESENT:
        return "not_present";
    case XF_ERR_INVALID_STATE:
        return "invalid_state";
    case XF_ERR_INVALID_ARG:
        return "invalid_arg";
    case XF_ERR_UNAVAILABLE:
        return "unavailable";
    }
    return "unknown";
}

#ifdef __cplusplus
}
#endif