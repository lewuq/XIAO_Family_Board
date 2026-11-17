/**
 * @file xf_idle.c
 * @brief Carousel schedule and inactivity rule, evaluated from injected time.
 *
 * No timer object is created here: the controller polls these predicates with a
 * monotonic millisecond timestamp, which keeps the rules deterministic and
 * testable on the host (design note section 8).
 */

#include <stdbool.h>
#include <stddef.h>

#include "xf_app_state.h"

bool xf_autoplay_due(const xf_app_state_t *state, uint64_t now_ms)
{
    if (state == NULL || state->screen != XF_SCREEN_ATTRACT) {
        return false;
    }
    /* A pressed pointer freezes the carousel: the visible identity must keep
     * matching what the user is about to open (design note 7). */
    if (state->pointer_down || state->external_touch_mask != 0U || state->autoplay_paused) {
        return false;
    }
    if (state->next_slide_deadline_ms == 0U) {
        return false;
    }
    return now_ms >= state->next_slide_deadline_ms;
}

void xf_autoplay_commit(xf_app_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return;
    }
    state->autoplay_index = xf_product_next(state->autoplay_index);
    state->current_product_id = state->autoplay_index;

    uint64_t scheduled = state->next_slide_deadline_ms + XF_AUTOPLAY_INTERVAL_MS;
    /* When the scheduler missed one or more deadlines, re-anchor instead of
     * playing several slides back to back (design note 8.7). */
    if (scheduled <= now_ms) {
        scheduled = now_ms + XF_AUTOPLAY_INTERVAL_MS;
    }
    state->next_slide_deadline_ms = scheduled;
}

void xf_autoplay_restart(xf_app_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return;
    }
    state->autoplay_index = (state->current_product_id < XF_PRODUCT_COUNT)
                            ? state->current_product_id
                            : 0U;
    state->current_product_id = state->autoplay_index;
    state->next_slide_deadline_ms = state->autoplay_paused
                                      ? 0U : now_ms + XF_AUTOPLAY_INTERVAL_MS;
}

uint16_t xf_autoplay_progress_permille(const xf_app_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return 0U;
    }
    /* A held finger pauses the carousel, so the bar holds the value it had when
     * the press landed instead of drifting or snapping back to zero. */
    if (state->pointer_down) {
        return state->slide_hold_permille;
    }
    if (state->autoplay_paused || state->next_slide_deadline_ms == 0U) {
        return 0U;
    }
    if (now_ms >= state->next_slide_deadline_ms) {
        return 1000U;
    }
    uint64_t start = state->next_slide_deadline_ms - XF_AUTOPLAY_INTERVAL_MS;
    if (now_ms <= start) {
        return 0U;
    }
    uint64_t elapsed = now_ms - start;
    uint64_t permille = (elapsed * 1000U) / XF_AUTOPLAY_INTERVAL_MS;
    return (uint16_t)(permille > 1000U ? 1000U : permille);
}

bool xf_idle_expired(const xf_app_state_t *state, uint64_t now_ms)
{
    if (state == NULL || state->screen == XF_SCREEN_BOOT ||
        (state->screen == XF_SCREEN_ATTRACT && !state->autoplay_paused)) {
        return false;
    }
    /* Holding a finger down must never trigger the inactivity recovery, and a
     * release that already happened starts a fresh window (design note 7). */
    if (state->pointer_down || state->external_touch_mask != 0U) {
        return false;
    }
    return (now_ms - state->last_input_release_ms) >= XF_IDLE_TIMEOUT_MS;
}