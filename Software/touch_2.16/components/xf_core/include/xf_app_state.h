/**
 * @file xf_app_state.h
 * @brief Single mutable business state, its reducer and the timer rules.
 *
 * xf_core is pure logic: it includes no LVGL, no driver header and performs no
 * I/O, so it runs in host unit tests. The UI controller is the only writer and
 * derives every widget appearance from this one state (code rules 4.1).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xf_config.h"
#include "xf_event.h"
#include "xf_product.h"
#include "xf_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Screens of the state machine (design note section 5). */
typedef enum {
    XF_SCREEN_BOOT = 0, /**< Waiting for the first committed frame. */
    XF_SCREEN_ATTRACT,  /**< Auto carousel, 3000 ms per product. */
    XF_SCREEN_CATALOG,  /**< Catalog browsing, tap opens DETAIL. */
    XF_SCREEN_DETAIL,   /**< One product, scrollable groups. */
    XF_SCREEN_SELECT,   /**< Catalog layout with selection semantics. */
    XF_SCREEN_COMPARE,  /**< 2-3 products, fixed header, tabbed fields. */
} xf_screen_t;

/** @brief Transient notice shown as one line above the bottom bar. */
typedef enum {
    XF_NOTICE_NONE = 0,
    XF_NOTICE_SELECTION_MIN,     /**< "Select at least 2 products". */
    XF_NOTICE_SELECTION_MAX,     /**< "Up to 3 products". */
    XF_NOTICE_SELECTION_EMPTY,   /**< "No products selected". */
    XF_NOTICE_SELECTION_READY,   /**< "{count} selected". */
    XF_NOTICE_IMAGE_UNAVAILABLE, /**< "Image unavailable". */
    XF_NOTICE_DATA_UNAVAILABLE,  /**< "Data unavailable". */
} xf_notice_t;

/** @brief Where the user came from, so DETAIL/COMPARE can restore the view. */
typedef struct {
    uint8_t screen;        /**< xf_screen_t to return to. */
    int8_t filter;         /**< Manufacturer filter at that time (-1 = all). */
    uint8_t category;      /**< COMPARE tab at that time. */
    int32_t scroll_y_px;   /**< Scroll offset at that time. */
} xf_return_context_t;

/** @brief The only mutable business state; owned by xf_ui_controller. */
typedef struct {
    uint8_t screen;                     /**< xf_screen_t */
    uint8_t current_product_id;         /**< Product shown by DETAIL/ATTRACT. */
    uint8_t detail_product_id;          /**< Owner of the last detail scroll position. */
    uint8_t autoplay_index;             /**< Position in the 18 item order. */
    uint8_t selected_ids[XF_COMPARE_CAPACITY]; /**< Insertion order, no gaps. */
    uint8_t selected_count;             /**< 0..XF_COMPARE_CAPACITY */
    int8_t manufacturer_filter;         /**< -1 = All, else 0..6 */
    uint8_t compare_category;           /**< xf_compare_category_t */
    bool compare_differences_only;      /**< Hide rows where all values match. */
    uint8_t language;                   /**< 0=English (boot default), 1=Chinese. */
    uint32_t external_touch_mask;        /**< Product-ID bits currently held on the board. */
    bool autoplay_paused;              /**< Hold the manually chosen model. */
    bool pointer_down;                  /**< True between TOUCH_DOWN and TOUCH_UP. */
    bool tap_cancelled;                 /**< Set by a drag; blocks the release tap. */
    uint8_t interaction_product_id;     /**< Frozen identity captured on press. */
    uint8_t notice;                     /**< xf_notice_t */
    uint64_t notice_until_ms;           /**< Notice expiry, monotonic ms. */
    uint64_t last_input_release_ms;     /**< Last release, for the idle rule. */
    uint64_t next_slide_deadline_ms;    /**< Monotonic deadline of the next slide. */
    uint16_t slide_hold_permille;       /**< Timer bar value frozen while pressed. */
    xf_return_context_t return_context; /**< Screen we should return to. */
} xf_app_state_t;

/** @brief Side effects the controller must apply after a reduction. */
typedef struct {
    bool screen_changed;   /**< Rebuild the page tree. */
    bool content_changed;  /**< Repaint the current page (selection, filter, tab). */
    bool slide_committed;  /**< A new carousel product became visible. */
    bool reset_slide_clock;/**< Restart the 3000 ms window from now. */
    bool clear_visitor;    /**< Idle recovery cleared selection and filter. */
    bool reset_detail_scroll; /**< Another product detail must start at the top. */
    xf_result_t result;    /**< Business outcome of the event. */
} xf_effects_t;

/** @brief Reset to BOOT with an empty selection and no deadlines. */
void xf_app_state_init(xf_app_state_t *state);

/**
 * @brief Apply one event to the application state.
 * @param[in,out] state State owned by the UI controller; NULL is rejected.
 * @param[in] event Immutable event with a monotonic timestamp in ms.
 * @param[out] effects Requested side effects; initialised by this function.
 * @return XF_OK when accepted, otherwise the documented business result.
 * @note Call only from the controller's serialized context.
 * @note Never touches LVGL, hardware, the clock or the filesystem.
 */
xf_result_t xf_app_reduce(xf_app_state_t *state, const xf_event_t *event, xf_effects_t *effects);

/**
 * @brief Resolve the product the current screen is about.
 * @param[in] state Current state.
 * @return Product identifier, or XF_PRODUCT_INVALID for an empty state.
 */
uint8_t xf_app_visible_product(const xf_app_state_t *state);

/** @brief Product-ID mask to highlight on the external touch board. */
uint32_t xf_app_displayed_products(const xf_app_state_t *state);

/** @brief Slave host-selection mask, with optional unattended carousel sync.
 * A zero mask requests CLEAR_SELECTION, preserving the slave's local touch.
 * UI display identity is independent and always stays available above. */
uint32_t xf_app_host_selected_products(const xf_app_state_t *state);

/* ----- Selection set (xf_compare.c) ------------------------------------- */

/**
 * @brief Test whether a product is part of the comparison.
 * @param[in] state Current state.
 * @param[in] product_id Candidate identifier.
 * @return true when present.
 */
bool xf_compare_contains(const xf_app_state_t *state, uint8_t product_id);

/**
 * @brief Add a product to the comparison, preserving insertion order.
 * @param[in,out] state Current state.
 * @param[in] product_id Candidate identifier.
 * @return XF_OK, XF_ERR_INVALID_ID, XF_ERR_DUPLICATE or XF_ERR_COMPARE_FULL.
 * @note The capacity check happens before any write, so adding a fourth product
 *       never disturbs the three already stored (design note 7).
 */
xf_result_t xf_compare_add(xf_app_state_t *state, uint8_t product_id);

/**
 * @brief Remove a product and close the gap in the order markers.
 * @param[in,out] state Current state.
 * @param[in] product_id Candidate identifier.
 * @return XF_OK or XF_ERR_NOT_PRESENT.
 */
xf_result_t xf_compare_remove(xf_app_state_t *state, uint8_t product_id);

/**
 * @brief Add when absent, remove when present.
 * @param[in,out] state Current state.
 * @param[in] product_id Candidate identifier.
 * @return Result of the performed operation.
 */
xf_result_t xf_compare_toggle(xf_app_state_t *state, uint8_t product_id);

/** @brief Drop the whole selection set. */
void xf_compare_clear(xf_app_state_t *state);

/** @brief Compare is allowed only with 2 or 3 distinct products. */
bool xf_compare_ready(const xf_app_state_t *state);

/**
 * @brief Fetch the nth selected product.
 * @param[in] state Current state.
 * @param[in] ordinal Index below selected_count.
 * @return Identifier, or XF_PRODUCT_INVALID when the ordinal is out of range.
 */
uint8_t xf_compare_product_at(const xf_app_state_t *state, uint8_t ordinal);

/**
 * @brief Decide whether one COMPARE row must be hidden by the difference filter.
 * @param[in] state Current state (2 or 3 selected products).
 * @param[in] field Spec field under test.
 * @return true when every selected product has the same verified value.
 * @note A row that contains pending or conflicting values is never "the same",
 *       because a missing value must not be presented as an equality
 *       (design note 9.2).
 */
bool xf_compare_row_is_identical(const xf_app_state_t *state, uint8_t field);

/* ----- Timers (xf_idle.c) ----------------------------------------------- */

/**
 * @brief Test whether the carousel deadline has been reached.
 * @param[in] state Current state.
 * @param[in] now_ms Monotonic timestamp in ms.
 * @return true when the slide should be committed.
 */
bool xf_autoplay_due(const xf_app_state_t *state, uint64_t now_ms);

/**
 * @brief Commit one slide and schedule the next one.
 * @param[in,out] state Current state; the caller must already know the slide is due.
 * @param[in] now_ms Monotonic timestamp in ms.
 * @note If several deadlines were missed, exactly one slide is committed and the
 *       next deadline is re-anchored to now; the carousel never bursts to catch
 *       up (design note 8.7).
 * @note The slide begins from the product that was last shown, so the timer
 *       never skips a slot even when a resource request failed.
 */
void xf_autoplay_commit(xf_app_state_t *state, uint64_t now_ms);

/**
 * @brief Restart the carousel window from `now_ms`.
 * @param[in,out] state Current state.
 * @param[in] now_ms Monotonic timestamp in ms.
 */
void xf_autoplay_restart(xf_app_state_t *state, uint64_t now_ms);

/**
 * @brief Progress of the current slide for the 2 px timer bar.
 * @param[in] state Current state.
 * @param[in] now_ms Monotonic timestamp in ms.
 * @return 0..1000, clamped; the value frozen at press time while a finger is
 *         down; 0 when no deadline is armed.
 */
uint16_t xf_autoplay_progress_permille(const xf_app_state_t *state, uint64_t now_ms);

/**
 * @brief Test whether the inactivity rule must fire.
 * @param[in] state Current state.
 * @param[in] now_ms Monotonic timestamp in ms.
 * @return true when the screen is an interaction screen, no pointer is down and
 *         the last release is older than XF_IDLE_TIMEOUT_MS.
 */
bool xf_idle_expired(const xf_app_state_t *state, uint64_t now_ms);

#ifdef __cplusplus
}
#endif