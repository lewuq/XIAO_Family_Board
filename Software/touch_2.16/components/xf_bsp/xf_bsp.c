/**
 * @file xf_bsp.c
 * @brief BSP orchestration: shared I2C bus, expander power sequence, boot log.
 *
 * Bring-up order and its rationale
 * --------------------------------
 * The panel and the touch controller are held in reset by the PCA9535, and the
 * panel rails are gated by the same expander. Nothing on the QSPI or touch side
 * can be probed before the expander has been opened and the rails are up, so the
 * order is fixed:
 *
 *   I2C bus -> expander -> rails -> reset release -> QSPI/CO5300 -> LVGL port
 *   -> CST9220 -> LVGL pointer device
 *
 * The resets are driven through the expander, therefore both the CO5300 and the
 * CST9220 driver are created with `reset_gpio_num = GPIO_NUM_NC`. Passing an
 * expander bit number as a native GPIO is exactly what the design note forbids.
 *
 * Failure policy
 * --------------
 * A display failure aborts `xf_bsp_init()` because a showcase without a screen
 * cannot run. A touch failure is reported as a warning: the pages still render
 * and the caller can decide what to show. Neither path reboots in a loop, and
 * no raw register or path value is printed to the user-facing UI.
 */

#include "xf_bsp_internal.h"

#include "esp_check.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_lcd_co5300.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "xf_board.h"

static const char *TAG = "xf_bsp";

/** Single application-wide instance; stages fill it progressively. */
static xf_bsp_t s_bsp;
static bool s_bsp_initialised;

/** Diagnostic counter incremented by the touch read path. */
static volatile uint32_t s_touch_errors;

xf_bsp_t *xf_bsp_instance(void)
{
    return &s_bsp;
}

const xf_bsp_t *xf_bsp_get(void)
{
    return s_bsp_initialised ? &s_bsp : NULL;
}

uint32_t xf_bsp_touch_error_count(void)
{
    return s_touch_errors;
}

void xf_bsp_touch_note_error(void)
{
    s_touch_errors++;
}

esp_err_t xf_bsp_i2c_open(xf_bsp_t *bsp)
{
    ESP_RETURN_ON_FALSE(bsp != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    const i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = XF_I2C_PORT,
        .sda_io_num = XF_PIN_I2C_SDA,
        .scl_io_num = XF_PIN_I2C_SCL,
        .glitch_ignore_cnt = 7,
        /* The board has external pull-ups on both lines (docs/pinmap.json). */
        .flags = {
            .enable_internal_pullup = false,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bsp->i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        bsp->i2c_bus = NULL;
        return err;
    }

    err = xf_pca9535_open(bsp->i2c_bus, XF_PCA9535_ADDRESS, &bsp->expander);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PCA9535 open at 0x%02X failed: %s", XF_PCA9535_ADDRESS, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C ready: SDA=%d SCL=%d %d Hz", XF_PIN_I2C_SDA, XF_PIN_I2C_SCL, XF_I2C_CLOCK_HZ);
    return ESP_OK;
}

/**
 * @brief Enable the three panel rails and release both reset lines.
 *
 * Both reset networks are active low, so the sequence is: assert reset, power
 * the rails, wait, release reset, wait again. The delays are conservative
 * bring-up values (see `xf_board.h`); the vendor timing table is still missing
 * (design note HW-02).
 */
esp_err_t xf_bsp_power_open(xf_bsp_t *bsp)
{
    ESP_RETURN_ON_FALSE(bsp != NULL && bsp->expander != NULL, ESP_ERR_INVALID_ARG, TAG, "expander not available");

    /* PCA9535 output latches can contain 0xFF after power-on or retain the
     * preceding firmware's state across an MCU reset. Preload low BEFORE
     * changing direction so reset/enable outputs cannot pulse high. Only the
     * five bits owned by this BSP are changed; all other expander bits survive. */
    static const struct {
        uint8_t port;
        uint8_t bit;
    } outputs[] = {
        {XF_EXP_RESET_PORT, XF_EXP_BIT_DISPLAY_RESET},
        {XF_EXP_RESET_PORT, XF_EXP_BIT_TOUCH_RESET},
        {XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_MODULE_EN},
        {XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_VBAT_EN},
        {XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_VCI_EN},
    };

    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, outputs[i].port, outputs[i].bit, false),
                            TAG, "preload output low failed");
        ESP_RETURN_ON_ERROR(xf_pca9535_configure_output(bsp->expander, outputs[i].port, outputs[i].bit),
                            TAG, "configure output failed");
    }

    /* Establish a known reset/disabled state, including after a warm reboot. */
    vTaskDelay(pdMS_TO_TICKS(XF_RESET_HOLD_MS));

    /* Supply switches first, then the panel's own VCI_EN, then reset release.
     * These are conservative bring-up waits, not measured voltage assertions. */
    ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_VCI_EN, true),
                        TAG, "enable VCI rail failed");
    ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_VBAT_EN, true),
                        TAG, "enable VBAT-labelled rail failed");
    vTaskDelay(pdMS_TO_TICKS(XF_POWER_RAIL_SETTLE_MS));

    ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, XF_EXP_POWER_PORT, XF_EXP_BIT_DISPLAY_MODULE_EN, true),
                        TAG, "enable module failed");
    vTaskDelay(pdMS_TO_TICKS(XF_POWER_RAIL_SETTLE_MS));

    ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, XF_EXP_RESET_PORT, XF_EXP_BIT_DISPLAY_RESET, true),
                        TAG, "release display reset failed");
    ESP_RETURN_ON_ERROR(xf_pca9535_write_pin(bsp->expander, XF_EXP_RESET_PORT, XF_EXP_BIT_TOUCH_RESET, true),
                        TAG, "release touch reset failed");

    vTaskDelay(pdMS_TO_TICKS(XF_RESET_BOOT_MS));

    ESP_LOGI(TAG, "panel enables asserted, resets released (rail/module wait %u ms each, reset %u/%u ms)",
             XF_POWER_RAIL_SETTLE_MS, XF_RESET_HOLD_MS, XF_RESET_BOOT_MS);
    return ESP_OK;
}

esp_err_t xf_bsp_init(void)
{
    ESP_RETURN_ON_FALSE(!s_bsp_initialised, ESP_ERR_INVALID_STATE, TAG, "BSP already initialised");

    esp_err_t err = xf_bsp_i2c_open(&s_bsp);
    if (err != ESP_OK) {
        /* Without the expander the panel cannot be powered: report and stop. */
        return err;
    }

    err = xf_bsp_power_open(&s_bsp);
    if (err != ESP_OK) {
        return err;
    }

    err = xf_bsp_display_open(&s_bsp);
    if (err != ESP_OK) {
        return err;
    }

    err = xf_bsp_touch_open(&s_bsp);
    if (err != ESP_OK) {
        /* Degraded but usable: pages render, input is missing. */
        ESP_LOGW(TAG, "touch unavailable (%s); continuing without input", esp_err_to_name(err));
        s_bsp.touch_ready = false;
    }

    s_bsp_initialised = true;
    ESP_LOGI(TAG, "BSP ready: display=480x480 QSPI@%d Hz, touch=%s", XF_QSPI_CLOCK_HZ,
             s_bsp.touch_ready ? "CST9220" : "absent");
    return ESP_OK;
}

esp_err_t xf_bsp_set_brightness(uint8_t percent)
{
    ESP_RETURN_ON_FALSE(s_bsp_initialised && s_bsp.panel != NULL, ESP_ERR_INVALID_STATE, TAG, "display not ready");
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "brightness %u out of range", percent);
    return esp_lcd_panel_co5300_set_brightness(s_bsp.panel, percent);
}

void xf_bsp_log_capabilities(void)
{
    uint32_t flash_size = 0;
    const esp_err_t flash_err = esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "resources: flash=%lu bytes (%s)", (unsigned long)flash_size,
             flash_err == ESP_OK ? "detected" : esp_err_to_name(flash_err));
    ESP_LOGI(TAG, "resources: heap internal=%u dma=%u largest_dma=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "resources: heap psram=%u (psram is %s)", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "available" : "disabled or unavailable");
    ESP_LOGI(TAG, "resources: lvgl heap configured=%u bytes, draw buffers=%u px each", (unsigned)LV_MEM_SIZE,
             (unsigned)XF_PANEL_BUFFER_PIXELS);
    ESP_LOGI(TAG, "stages: display=%s touch=%s touch_errors=%lu",
             s_bsp.display_ready ? "ok" : "failed", s_bsp.touch_ready ? "ok" : "failed",
             (unsigned long)s_touch_errors);
    /* Enable states and successful host writes do not measure panel voltages
     * or establish that the write-only panel accepted the commands. */
    ESP_LOGI(TAG, "panel path: host configured; startup opens the UI without colour-sweep testing");
    ESP_LOGI(TAG, "unverified items: panel-specific init, rail voltages, orientation/RGB order, QSPI limit");
}

/* External V1 device shares the existing GPIO5/6 master bus. The protocol's
 * GPIO8/9 numbers belong to the other PCB and must not replace these pins. */
static i2c_master_dev_handle_t s_external_device;
static SemaphoreHandle_t s_external_lock;
static uint8_t s_external_address;
#define XF_EXTERNAL_TIMEOUT_MS 20

esp_err_t xf_bsp_external_open(uint8_t address, uint32_t speed_hz)
{
    ESP_RETURN_ON_FALSE(s_bsp_initialised && address >= 0x08U && address <= 0x77U &&
                        speed_hz > 0U && speed_hz <= 400000U,
                        ESP_ERR_INVALID_ARG, TAG, "invalid external device setup");
    if (s_external_device != NULL) {
        return address == s_external_address ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    s_external_lock = xSemaphoreCreateMutex();
    if (s_external_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = speed_hz,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bsp.i2c_bus, &config, &s_external_device);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_external_lock);
        s_external_lock = NULL;
        return err;
    }
    s_external_address = address;
    return ESP_OK;
}

esp_err_t xf_bsp_external_probe(void)
{
    if (s_external_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_probe(s_bsp.i2c_bus, s_external_address, XF_EXTERNAL_TIMEOUT_MS);
}

/* V1 requires STOP after the register pointer write. A combined
 * transmit_receive would use repeated START and is deliberately not used. */
static esp_err_t external_read_locked(uint8_t reg, uint8_t out[4])
{
    esp_err_t err = i2c_master_transmit(s_external_device, &reg, 1, XF_EXTERNAL_TIMEOUT_MS);
    if (err == ESP_OK) {
        err = i2c_master_receive(s_external_device, out, 4, XF_EXTERNAL_TIMEOUT_MS);
    }
    return err;
}

esp_err_t xf_bsp_external_read(uint8_t reg, uint8_t out[4])
{
    if (s_external_device == NULL || s_external_lock == NULL || out == NULL || reg > 2U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_external_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = external_read_locked(reg, out);
    xSemaphoreGive(s_external_lock);
    return err;
}

esp_err_t xf_bsp_external_command(const uint8_t *data, size_t length, uint8_t status[4])
{
    if (s_external_device == NULL || s_external_lock == NULL ||
        data == NULL || status == NULL || length == 0U || length > 5U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_external_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    /* Keep command and STATUS.RESULT together relative to other external
     * calls. Internal touch/expander transactions can still use the bus safely. */
    esp_err_t err = i2c_master_transmit(s_external_device, data, length, XF_EXTERNAL_TIMEOUT_MS);
    if (err == ESP_OK) {
        err = external_read_locked(0x00U, status);
    }
    xSemaphoreGive(s_external_lock);
    return err;
}
