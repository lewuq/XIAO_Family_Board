/**
 * @file xf_pca9535.h
 * @brief Minimal, bit-safe PCA9535 driver used for the display power sequence.
 *
 * Why a dedicated driver instead of generic GPIO plumbing
 * ------------------------------------------------------
 * The display reset lines and the three power enables of this board are not
 * ESP32-S3 GPIOs, they are behind a PCA9535 I/O expander on the shared I2C bus
 * (design note section 3.2). The design note requires that the BSP performs the
 * expander reset and power control itself and that the driver configurations do
 * not try to drive an expander bit as a native GPIO.
 *
 * The expander also drives pins this application does not own. Therefore every
 * write is a read-modify-write of the *shadow* output register that was read
 * back at open time, and only the bit that a caller names is touched. The
 * shadow is refreshed on open and never rebuilt from assumptions about the
 * hardware defaults.
 *
 * Threading and ownership
 * -----------------------
 * All functions are thread-safe. The I2C master device handle serialises
 * transactions internally, and the shadow registers are protected by one
 * mutex owned by the instance. The instance is created by
 * `xf_pca9535_open()` and lives for the whole application lifetime; there is no
 * release path because the expander must not be left with our power enables
 * released while the application is running.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Expander instance.
 *
 * The fields are private to `xf_pca9535.c`; the structure is declared in the
 * header only so callers can hold a pointer without dynamic allocation.
 */
typedef struct xf_pca9535 xf_pca9535_t;

/**
 * @brief Open the expander on an existing I2C bus.
 *
 * Reads the current output-latch and configuration registers so that later
 * bit writes preserve every pin this application does not use.
 *
 * @param[in]  bus          Initialised I2C master bus handle. Must stay alive.
 * @param[in]  address      7-bit I2C address of the expander.
 * @param[out] out_expander Receives the instance. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG for a NULL pointer or an out-of-range address
 *      - ESP_ERR_NO_MEM when the instance cannot be allocated
 *      - ESP_ERR_NOT_FOUND / I2C errors when the device does not answer
 *
 * @note Not thread-safe with respect to concurrent `xf_pca9535_open()` calls.
 */
esp_err_t xf_pca9535_open(i2c_master_bus_handle_t bus, uint8_t address, xf_pca9535_t **out_expander);

/**
 * @brief Configure one pin as a push-pull output, keeping its current level.
 *
 * @param[in] expander Instance from `xf_pca9535_open()`.
 * @param[in] port     Port index, 0 or 1.
 * @param[in] bit      Bit index, 0..7.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a bad port/bit or NULL.
 */
esp_err_t xf_pca9535_configure_output(xf_pca9535_t *expander, uint8_t port, uint8_t bit);

/**
 * @brief Drive one pin of an already configured output.
 *
 * @param[in] expander Instance from `xf_pca9535_open()`.
 * @param[in] port     Port index, 0 or 1.
 * @param[in] bit      Bit index, 0..7.
 * @param[in] level    true drives the pin high, false drives it low.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a bad port/bit or NULL,
 *         otherwise the I2C error from the write.
 */
esp_err_t xf_pca9535_write_pin(xf_pca9535_t *expander, uint8_t port, uint8_t bit, bool level);

/**
 * @brief Read the raw input port register.
 *
 * Used for diagnostics only (for example to confirm that a reset line is
 * released). The input register reflects the pin level regardless of the
 * configured direction.
 *
 * @param[in]  expander Instance from `xf_pca9535_open()`.
 * @param[in]  port     Port index, 0 or 1.
 * @param[out] out_value Receives the 8-bit port value.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on bad arguments.
 */
esp_err_t xf_pca9535_read_inputs(xf_pca9535_t *expander, uint8_t port, uint8_t *out_value);

#ifdef __cplusplus
}
#endif