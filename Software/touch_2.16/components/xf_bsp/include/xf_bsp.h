/**
 * @file xf_bsp.h
 * @brief Board support API: power sequencing, CO5300 display and CST9220 touch.
 *
 * Layering
 * --------
 * `xf_bsp` is the only component allowed to talk to GPIO, I2C, QSPI or the LVGL
 * port. It owns the panel and touch objects for the whole run and exposes them
 * so `xf_ui` can create pages on top of the initialised display.
 *
 * What this component deliberately does not do
 * --------------------------------------------
 * - It does not create screens, widgets or styles.
 * - It does not read product data, strings or assets.
 * - It does not decide brightness policy per page; `xf_bsp_set_brightness()`
 *   is a primitive that the UI layer may call.
 *
 * Unverified hardware facts
 * -------------------------
 * The design note lists several open bring-up items (HW-01..HW-05): power-rail
 * compatibility, the vendor init sequence, the interaction of the CST9220 I2C
 * address/register map and the physical orientation/RGB order. The
 * configuration built here is a documented candidate, not a verified hardware
 * requirement. `xf_bsp_log_capabilities()` prints everything needed to review
 * it on a real board.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#include "xf_pca9535.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Immutable view of the initialised board resources.
 *
 * A field is NULL until the corresponding stage succeeded. `display_ready` and
 * `touch_ready` let the UI decide between a normal start and a degraded start
 * instead of aborting the whole application on a single bus failure.
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;        /*!< Shared I2C bus (touch + expander). */
    xf_pca9535_t *expander;                 /*!< PCA9535 power/reset expander. */
    esp_lcd_panel_io_handle_t panel_io;     /*!< QSPI panel IO for the CO5300. */
    esp_lcd_panel_handle_t panel;           /*!< CO5300 panel handle. */
    lv_display_t *display;                  /*!< LVGL display created by the port. */
    esp_lcd_panel_io_handle_t touch_io;     /*!< I2C panel IO for the CST9220. */
    esp_lcd_touch_handle_t touch;           /*!< CST9220 touch handle. */
    lv_indev_t *indev;                      /*!< LVGL pointer device bound to the touch. */
    bool display_ready;                     /*!< True when the display pipeline works. */
    bool touch_ready;                       /*!< True when the input pipeline works. */
} xf_bsp_t;

/**
 * @brief Bring the board up: I2C, expander power sequence, QSPI, LVGL port.
 *
 * Order of operations, and why:
 *  1. I2C bus + PCA9535, because the panel and the touch controller are held in
 *     reset by the expander until the rails are up.
 *  2. Power enables, then reset release, then a settle delay.
 *  3. QSPI bus, panel IO and CO5300, then `lvgl_port_init()`/`lvgl_port_add_disp()`.
 *  4. CST9220 plus the LVGL pointer device.
 *
 * A failing display stage is returned as an error because the showcase has no
 * meaningful degraded mode without a screen. A failing touch stage is logged and
 * reported through `touch_ready == false` so the pages can still render.
 *
 * @return
 *      - ESP_OK when the display pipeline is usable
 *      - ESP_ERR_INVALID_STATE when called twice
 *      - other ESP-IDF errors of the stage that failed
 */
esp_err_t xf_bsp_init(void);

/**
 * @brief Access the initialised board resources.
 *
 * @return Pointer owned by the BSP, valid after a successful `xf_bsp_init()`,
 *         NULL before it. The pointer and its contents must be treated as
 *         read-only by callers.
 */
const xf_bsp_t *xf_bsp_get(void);

/**
 * @brief Set the panel brightness through the CO5300 command path.
 *
 * There is no backlight PWM pin on this module (design note HW-05), so
 * brightness is a register write to the panel.
 *
 * @param[in] percent Brightness 0..100.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a bad percentage,
 *         ESP_ERR_INVALID_STATE before `xf_bsp_init()`.
 */
esp_err_t xf_bsp_set_brightness(uint8_t percent);

/**
 * @brief Log flash size, heap pools and the display/touch stage results.
 *
 * The design note (section 10.3) requires the real partition size, available
 * heap, DMA-capable memory and PSRAM availability to be recorded in the boot
 * log instead of assuming the nominal resource figures of the module.
 *
 * @note Must be called after `xf_bsp_init()`; safe to call at any time
 *       afterwards. Never returns an error: this is a diagnostic.
 */
void xf_bsp_log_capabilities(void);

/**
 * @brief Touch read error counter, for diagnostics and for the UI self-check.
 *
 * Incremented for every read attempt that produced an error after all retries.
 *
 * @return Number of failed touch reads since boot.
 */
uint32_t xf_bsp_touch_error_count(void);

/** @brief Start ES8311/I2S speaker feedback; failure is nonfatal to the UI. */
esp_err_t xf_bsp_audio_start(void);
/** @brief Queue one short beep without blocking; task context, not ISR context. */
esp_err_t xf_bsp_audio_beep(void);

/** @brief Attach an external slave to the existing master bus; no pin remap. */
esp_err_t xf_bsp_external_open(uint8_t address, uint32_t speed_hz);
/** @brief Quiet presence probe; a missing board does not stop the display. */
esp_err_t xf_bsp_external_probe(void);
/** @brief V1 register pointer write + STOP + one complete four-byte read. */
esp_err_t xf_bsp_external_read(uint8_t reg, uint8_t out[4]);
/** @brief Write a complete V1 command, then read four-byte STATUS under one lock. */
esp_err_t xf_bsp_external_command(const uint8_t *data, size_t length, uint8_t status[4]);

#ifdef __cplusplus
}
#endif