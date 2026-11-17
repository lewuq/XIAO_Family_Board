/**
 * @file xf_touch_protocol.h
 * @brief Pure V1 wire helpers for XIAO_Touch_I2C_Protocol_v1.md.
 *
 * No driver, allocation or UI dependency. KEY_ID is the physical K number:
 * K9 is absent; product IDs remain contiguous and must never be used as wire
 * KEY_ID values without these conversion functions.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xf_product_ids.h"

#define XF_TP_ADDRESS 0x42U
#define XF_TP_SPEED_HZ 100000U
#define XF_TP_POLL_MS 20U
#define XF_TP_VERSION 1U
#define XF_TP_STATUS 0x00U
#define XF_TP_EVENT_HEAD 0x01U
#define XF_TP_KEY_STATE 0x02U
#define XF_TP_SELECT_DEVICE 0x10U
#define XF_TP_SET_RGB 0x11U
#define XF_TP_CLEAR_SELECTION 0x12U
#define XF_TP_ACK_EVENT 0x20U
#define XF_TP_CLEAR_EVENTS 0x21U
#define XF_TP_READY 0x01U
#define XF_TP_HAS_EVENTS 0x02U
#define XF_TP_OVERFLOW 0x04U
#define XF_TP_HOST_SELECTED 0x08U
#define XF_TP_RGB_FAULT 0x10U
#define XF_TP_DOWN 0x01U
#define XF_TP_UP 0x00U
#define XF_TP_EMPTY 0xFFU
#define XF_TP_VALID_KEY_MASK UINT32_C(0x0007FDFF)
#define XF_TP_VALID_PRODUCT_MASK ((UINT32_C(1) << XF_PRODUCT_COUNT) - 1U)

typedef struct {
    uint8_t version, flags, count, result;
} xf_tp_status_t;

typedef struct {
    uint8_t action, key;
    uint16_t seq;
} xf_tp_event_t;

typedef struct {
    bool delivered;
    xf_tp_event_t event;
} xf_tp_pending_t;

static inline uint8_t xf_tp_product_to_key(uint8_t product)
{
    /* Explicit designation protects the protocol if display ordering changes. */
    static const uint8_t keys[XF_PRODUCT_COUNT] = {
        [XF_PRODUCT_SAMD21] = 0, [XF_PRODUCT_SAMD21_PLUS] = 1,
        [XF_PRODUCT_STM32C5] = 2, [XF_PRODUCT_MG24_SENSE] = 3,
        [XF_PRODUCT_RP2350] = 4, [XF_PRODUCT_RP2040] = 5,
        [XF_PRODUCT_RP2040_PLUS] = 6, [XF_PRODUCT_RA4M1] = 7,
        [XF_PRODUCT_ESP32_C3] = 8, [XF_PRODUCT_ESP32_C5] = 10,
        [XF_PRODUCT_ESP32_C6] = 11, [XF_PRODUCT_ESP32_S3_PLUS] = 12,
        [XF_PRODUCT_ESP32_S3_SENSE] = 13, [XF_PRODUCT_NRF52840_SENSE] = 14,
        [XF_PRODUCT_NRF52840_PLUS] = 15, [XF_PRODUCT_NRF54L15_SENSE] = 16,
        [XF_PRODUCT_NRF54LM20A_SENSE] = 17, [XF_PRODUCT_NRF54LM20B_SENSE] = 18,
    };
    return product < XF_PRODUCT_COUNT ? keys[product] : XF_PRODUCT_INVALID;
}

static inline uint8_t xf_tp_key_to_product(uint8_t key)
{
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        if (xf_tp_product_to_key(id) == key) {
            return id;
        }
    }
    return XF_PRODUCT_INVALID;
}

static inline uint32_t xf_tp_u32(const uint8_t p[4])
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline bool xf_tp_decode_status(const uint8_t p[4], xf_tp_status_t *s)
{
    if (p == NULL || s == NULL || p[0] != XF_TP_VERSION ||
        (p[1] & 0xE0U) != 0U || p[2] > 32U || p[3] > 6U) {
        return false;
    }
    *s = (xf_tp_status_t){p[0], p[1], p[2], p[3]};
    return true;
}

static inline bool xf_tp_decode_event(const uint8_t p[4], xf_tp_event_t *e)
{
    if (p == NULL || e == NULL) {
        return false;
    }
    *e = (xf_tp_event_t){p[0], p[1], (uint16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8))};
    if (e->action == XF_TP_EMPTY) {
        return e->key == XF_TP_EMPTY && e->seq == 0U;
    }
    return (e->action == XF_TP_DOWN || e->action == XF_TP_UP) &&
           xf_tp_key_to_product(e->key) != XF_PRODUCT_INVALID;
}

static inline uint32_t xf_tp_keys_to_products(uint32_t keys)
{
    uint32_t result = 0;
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        if ((keys & (UINT32_C(1) << xf_tp_product_to_key(id))) != 0U) {
            result |= UINT32_C(1) << id;
        }
    }
    return result;
}

static inline uint32_t xf_tp_products_to_keys(uint32_t products)
{
    uint32_t result = 0;
    for (uint8_t id = 0; id < XF_PRODUCT_COUNT; ++id) {
        if ((products & (UINT32_C(1) << id)) != 0U) {
            result |= UINT32_C(1) << xf_tp_product_to_key(id);
        }
    }
    return result;
}

static inline bool xf_tp_already_delivered(const xf_tp_pending_t *p, const xf_tp_event_t *e)
{
    return p != NULL && e != NULL && p->delivered &&
           p->event.seq == e->seq && p->event.key == e->key &&
           p->event.action == e->action;
}

static inline void xf_tp_ack_bytes(uint16_t seq, uint8_t p[3])
{
    p[0] = XF_TP_ACK_EVENT;
    p[1] = (uint8_t)seq;
    p[2] = (uint8_t)(seq >> 8);
}
