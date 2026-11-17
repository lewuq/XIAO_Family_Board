/**
 * @file xf_config.h
 * @brief Central configuration of every timing and capacity in the product.
 *
 * Design note section 8 requires the carousel interval, the inactivity timeout
 * and the transition duration to be managed in one place. Every value carries
 * its unit in the name; microsecond, tick and millisecond values must not be
 * mixed (code rules 4.3).
 */

#pragma once

/** @brief Carousel period: delay between two slide starts, transition included. */
#define XF_AUTOPLAY_INTERVAL_MS 3000U

/** @brief Inactivity timeout that returns every interaction screen to ATTRACT. */
#define XF_IDLE_TIMEOUT_MS 6000U

/**
 * @brief Follow unattended carousel images with slave host-selection LEDs.
 * 0 (default): entering autoplay clears the previous host selection once;
 *              subsequent automatic slides send no LED-selection changes.
 * 1: automatic slides select the displayed product on the external board.
 * Manual selection/comparison, slave touch feedback and event polling remain
 * enabled in either mode. Change this macro and rebuild/flash to take effect.
 */
#ifndef XF_ENABLE_AUTOPLAY_LED_SYNC
#define XF_ENABLE_AUTOPLAY_LED_SYNC 0
#endif
#if XF_ENABLE_AUTOPLAY_LED_SYNC != 0 && XF_ENABLE_AUTOPLAY_LED_SYNC != 1
#error "XF_ENABLE_AUTOPLAY_LED_SYNC must be 0 or 1"
#endif

/** @brief Default page transition duration (design range 160-200 ms). */
#define XF_TRANSITION_MS 180U

/** @brief Maximum delay before a press shows its visual feedback. */
#define XF_PRESS_FEEDBACK_MS 80U

/** @brief Movement above this distance cancels a tap and becomes a drag. */
#define XF_TOUCH_SLOP_PX 12
/** Horizontal displacement needed for one completed carousel swipe. */
#define XF_SWIPE_THRESHOLD_PX 40

/** @brief How long a transient notice (for example the 3-product limit) stays. */
#define XF_NOTICE_DURATION_MS 2500U

/** @brief Polling period handed to the LVGL port timer. */
#define XF_UI_TICK_PERIOD_MS 5U

/** @brief Period of the controller heartbeat that evaluates timers. */
#define XF_CONTROLLER_TICK_MS 20U

/** @brief Number of products one comparison may hold. */
#define XF_COMPARE_CAPACITY_LOCAL 3U