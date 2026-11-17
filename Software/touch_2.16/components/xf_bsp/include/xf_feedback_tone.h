/**
 * @file xf_feedback_tone.h
 * @brief Bounded PCM for touch confirmation; no media files or floating point.
 */
#pragma once
#include <stdint.h>

#define XF_BEEP_SAMPLE_RATE_HZ 16000U
#define XF_BEEP_FREQUENCY_HZ 600U
#define XF_BEEP_DURATION_MS 40U
#define XF_BEEP_FRAMES (XF_BEEP_SAMPLE_RATE_HZ * XF_BEEP_DURATION_MS / 1000U)
#define XF_BEEP_FADE_FRAMES (XF_BEEP_SAMPLE_RATE_HZ * 5U / 1000U)
#define XF_BEEP_PEAK 8192

/** 600 Hz at 16 kHz, 5 ms fade at both ends to suppress clicks.
 * An 80-point cycle advances three entries per sample: 600 Hz exactly, with
 * no truncated 26/27-sample period. Returns zero outside the 40 ms envelope. */
static inline int16_t xf_feedback_sample(uint32_t frame)
{
    static const int16_t sine[80] = {
        0, 2571, 5126, 7649, 10126, 12539, 14876, 17121, 19260, 21280,
        23170, 24916, 26509, 27938, 29196, 30273, 31163, 31862, 32364, 32666,
        32767, 32666, 32364, 31862, 31163, 30273, 29196, 27938, 26509, 24916,
        23170, 21280, 19260, 17121, 14876, 12539, 10126, 7649, 5126, 2571,
        0, -2571, -5126, -7649, -10126, -12539, -14876, -17121, -19260, -21280,
        -23170, -24916, -26509, -27938, -29196, -30273, -31163, -31862, -32364, -32666,
        -32767, -32666, -32364, -31862, -31163, -30273, -29196, -27938, -26509, -24916,
        -23170, -21280, -19260, -17121, -14876, -12539, -10126, -7649, -5126, -2571
    };
    if (frame >= XF_BEEP_FRAMES) {
        return 0;
    }
    uint32_t gain = XF_BEEP_FADE_FRAMES;
    if (frame < gain) {
        gain = frame;
    }
    uint32_t tail = XF_BEEP_FRAMES - frame - 1U;
    if (tail < gain) {
        gain = tail;
    }
    uint32_t phase = (frame * XF_BEEP_FREQUENCY_HZ * 80U /
                      XF_BEEP_SAMPLE_RATE_HZ) % 80U;
    int32_t sample = (int32_t)sine[phase] * XF_BEEP_PEAK / 32767;
    return (int16_t)(sample * (int32_t)gain / (int32_t)XF_BEEP_FADE_FRAMES);
}
