/**
 * @file xf_pca9535.c
 * @brief Bit-safe PCA9535 I/O expander driver (registers of interest only).
 *
 * Register map used here
 * ----------------------
 * | Address | Meaning                      |
 * |---------|------------------------------|
 * | 0x02/03 | output latch, port 0 / port 1 |
 * | 0x06/07 | configuration, port 0 / 1     |
 * | 0x00/01 | input port, port 0 / port 1   |
 *
 * A configuration bit of 0 means "output", 1 means "input". The driver keeps an
 * 8-bit shadow of both output latches and both configuration registers so a
 * single-bit change never overwrites a neighbouring pin that belongs to another
 * function (the design note requires preserving unused expander bits).
 *
 * All accesses go through one mutex; the I2C master driver also serialises the
 * bus itself, but the shadow registers need their own guard so a
 * read-modify-write cannot interleave with another task.
 */

#include "xf_pca9535.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "xf_board.h"

static const char *TAG = "xf_pca9535";

/** PCA9535 register offsets for the two ports. */
#define XF_PCA9535_REG_INPUT (0x00U)
#define XF_PCA9535_REG_OUTPUT (0x02U)
#define XF_PCA9535_REG_CONFIG (0x06U)

/** Largest I2C address accepted by the 7-bit expander family. */
#define XF_PCA9535_MAX_ADDRESS (0x7FU)

struct xf_pca9535 {
    i2c_master_dev_handle_t device;
    SemaphoreHandle_t lock;
    uint8_t output[2]; /*!< Shadow of the output latch registers. */
    uint8_t config[2]; /*!< Shadow of the configuration registers. */
};

static esp_err_t pca9535_read_register(xf_pca9535_t *expander, uint8_t reg, uint8_t *out_value)
{
    ESP_RETURN_ON_FALSE(expander != NULL && out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return i2c_master_transmit_receive(expander->device, &reg, sizeof(reg), out_value, sizeof(*out_value), 100);
}

static esp_err_t pca9535_write_register(xf_pca9535_t *expander, uint8_t reg, uint8_t value)
{
    ESP_RETURN_ON_FALSE(expander != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    const uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(expander->device, payload, sizeof(payload), 100);
}

esp_err_t xf_pca9535_open(i2c_master_bus_handle_t bus, uint8_t address, xf_pca9535_t **out_expander)
{
    ESP_RETURN_ON_FALSE(bus != NULL && out_expander != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(address <= XF_PCA9535_MAX_ADDRESS, ESP_ERR_INVALID_ARG, TAG, "invalid address 0x%02X",
                        address);
    *out_expander = NULL;

    xf_pca9535_t *expander = calloc(1, sizeof(xf_pca9535_t));
    ESP_RETURN_ON_FALSE(expander != NULL, ESP_ERR_NO_MEM, TAG, "no memory for the expander instance");

    expander->lock = xSemaphoreCreateMutex();
    if (expander->lock == NULL) {
        free(expander);
        return ESP_ERR_NO_MEM;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = XF_I2C_CLOCK_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &device_config, &expander->device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device 0x%02X failed: %s", address, esp_err_to_name(err));
        vSemaphoreDelete(expander->lock);
        free(expander);
        return err;
    }

    /* Cache the current hardware state before any write happens. */
    for (uint8_t port = 0; port < 2; port++) {
        err = pca9535_read_register(expander, XF_PCA9535_REG_OUTPUT + port, &expander->output[port]);
        if (err == ESP_OK) {
            err = pca9535_read_register(expander, XF_PCA9535_REG_CONFIG + port, &expander->config[port]);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read port %u state failed: %s", port, esp_err_to_name(err));
            i2c_master_bus_rm_device(expander->device);
            vSemaphoreDelete(expander->lock);
            free(expander);
            return err;
        }
    }

    ESP_LOGI(TAG, "expander 0x%02X ready (output %02X/%02X, config %02X/%02X)", address, expander->output[0],
             expander->output[1], expander->config[0], expander->config[1]);
    *out_expander = expander;
    return ESP_OK;
}

esp_err_t xf_pca9535_configure_output(xf_pca9535_t *expander, uint8_t port, uint8_t bit)
{
    ESP_RETURN_ON_FALSE(expander != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(port < 2 && bit < 8, ESP_ERR_INVALID_ARG, TAG, "invalid port %u bit %u", port, bit);
    ESP_RETURN_ON_FALSE(xSemaphoreTake(expander->lock, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "lock timeout");

    esp_err_t err = ESP_OK;
    const uint8_t mask = (uint8_t)(1U << bit);
    if (expander->config[port] & mask) {
        const uint8_t updated = (uint8_t)(expander->config[port] & ~mask);
        err = pca9535_write_register(expander, XF_PCA9535_REG_CONFIG + port, updated);
        if (err == ESP_OK) {
            expander->config[port] = updated;
        }
    }

    xSemaphoreGive(expander->lock);
    return err;
}

esp_err_t xf_pca9535_write_pin(xf_pca9535_t *expander, uint8_t port, uint8_t bit, bool level)
{
    ESP_RETURN_ON_FALSE(expander != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(port < 2 && bit < 8, ESP_ERR_INVALID_ARG, TAG, "invalid port %u bit %u", port, bit);
    ESP_RETURN_ON_FALSE(xSemaphoreTake(expander->lock, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "lock timeout");

    const uint8_t mask = (uint8_t)(1U << bit);
    uint8_t updated = expander->output[port];
    if (level) {
        updated = (uint8_t)(updated | mask);
    } else {
        updated = (uint8_t)(updated & ~mask);
    }

    esp_err_t err = ESP_OK;
    if (updated != expander->output[port]) {
        err = pca9535_write_register(expander, XF_PCA9535_REG_OUTPUT + port, updated);
        if (err == ESP_OK) {
            expander->output[port] = updated;
        }
    }

    xSemaphoreGive(expander->lock);
    return err;
}

esp_err_t xf_pca9535_read_inputs(xf_pca9535_t *expander, uint8_t port, uint8_t *out_value)
{
    ESP_RETURN_ON_FALSE(expander != NULL && out_value != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(port < 2, ESP_ERR_INVALID_ARG, TAG, "invalid port %u", port);
    ESP_RETURN_ON_FALSE(xSemaphoreTake(expander->lock, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "lock timeout");

    const esp_err_t err = pca9535_read_register(expander, XF_PCA9535_REG_INPUT + port, out_value);

    xSemaphoreGive(expander->lock);
    return err;
}