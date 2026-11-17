/**
 * @file xf_bsp_internal.h
 * @brief Private contract between the BSP translation units.
 *
 * The BSP is split into a power/bus file, a display file and a touch file. They
 * share one instance owned by `xf_bsp.c`; the functions below are the only way a
 * sibling translation unit may reach it, which keeps the stage ordering in one
 * place and keeps the public header free of mutable accessors.
 *
 * Not part of the component include directory: `xf_ui` must not see this file.
 */

#pragma once

#include "xf_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mutable access to the BSP instance.
 *
 * @return Pointer to the static instance. Never NULL; stages fill it in.
 *
 * @note Only valid for the BSP translation units. Callers must not hand the
 *       returned pointer to other components.
 */
xf_bsp_t *xf_bsp_instance(void);

/**
 * @brief Create the shared I2C bus and open the PCA9535.
 *
 * @param[in,out] bsp Instance to fill in (`i2c_bus`, `expander`).
 * @return ESP_OK on success, otherwise the first failing I2C/expander error.
 */
esp_err_t xf_bsp_i2c_open(xf_bsp_t *bsp);

/**
 * @brief Run the expander power and reset sequence.
 *
 * Enables the three rails, releases both resets and waits for the module to
 * settle. Only the expander bits owned by this application are modified.
 *
 * @param[in,out] bsp Instance with a valid `expander`.
 * @return ESP_OK on success, otherwise the expander write error.
 */
esp_err_t xf_bsp_power_open(xf_bsp_t *bsp);

/**
 * @brief Initialise QSPI, the CO5300 and the LVGL display port.
 *
 * Fills `panel_io`, `panel`, `display` and `display_ready`.
 *
 * @param[in,out] bsp Instance from `xf_bsp_instance()`.
 * @return ESP_OK when LVGL has a display, otherwise the failing stage error.
 */
esp_err_t xf_bsp_display_open(xf_bsp_t *bsp);

/**
 * @brief Initialise the CST9220 and register the LVGL pointer device.
 *
 * Fills `touch_io`, `touch`, `indev` and `touch_ready`.
 *
 * @param[in,out] bsp Instance with a valid `i2c_bus` and a display to bind to.
 * @return ESP_OK when LVGL has a pointer device, otherwise the failing error.
 */
esp_err_t xf_bsp_touch_open(xf_bsp_t *bsp);

/**
 * @brief Increment the touch read error counter.
 *
 * Called by the touch read path after all retries of one sample failed.
 */
void xf_bsp_touch_note_error(void);

#ifdef __cplusplus
}
#endif