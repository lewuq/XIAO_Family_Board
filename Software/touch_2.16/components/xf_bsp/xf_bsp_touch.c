/**
 * @file xf_bsp_touch.c
 * @brief CST9220 touch bring-up and the LVGL pointer device.
 *
 * Why the pointer device is registered here instead of using
 * `lvgl_port_add_touch()`
 * --------------------------------------------------------------------
 * The port helper calls `esp_lcd_touch_read_data()` with `ESP_ERROR_CHECK`, so a
 * single failed I2C sample aborts the LVGL task. The design note (section 10.4)
 * instead requires a bounded retry count, a diagnosable log and no continuous
 * stream of synthetic clicks from a failing touch path. Registering the input
 * device directly keeps that recovery policy in the BSP, where the hardware
 * error can be counted, while LVGL still calls the read callback from its own
 * task, so no extra locking is needed.
 *
 * Error policy
 * ------------
 * - Each sample gets `XF_TOUCH_READ_RETRIES + 1` attempts.
 * - On total failure the sample is reported as released; no press is invented,
 *   so no tap is manufactured from a bus error.
 * - Logging is rate limited to one line per `XF_TOUCH_ERROR_LOG_INTERVAL_MS` to
 *   keep a broken bus from flooding the console.
 *
 * Reset handling
 * --------------
 * TP-RST is a PCA9535 bit (P01) that `xf_bsp_power_open()` already pulsed, so
 * the driver is created with `rst_gpio_num = GPIO_NUM_NC` and performs only its
 * software reset path.
 */

#include "xf_bsp_internal.h"

#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst9220.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "xf_board.h"

static const char *TAG = "xf_bsp_touch";

/** Time of the last rate-limited error log, in microseconds. */
static int64_t s_last_error_log_us;

/**
 * @brief Sample the controller with a bounded retry count.
 *
 * @param[in]  touch  Touch handle.
 * @param[out] points Receives up to `XF_TOUCH_MAX_POINTS` points.
 * @param[out] count  Receives the number of valid points.
 *
 * @return ESP_OK when a sample was obtained (possibly zero points), otherwise
 *         the last error. A zero point count is not an error: the finger is up.
 */
static esp_err_t touch_sample(esp_lcd_touch_handle_t touch, esp_lcd_touch_point_data_t *points, uint8_t *count)
{
    esp_err_t err = ESP_FAIL;

    for (uint32_t attempt = 0; attempt <= XF_TOUCH_READ_RETRIES; attempt++) {
        err = esp_lcd_touch_read_data(touch);
        if (err == ESP_OK) {
            err = esp_lcd_touch_get_data(touch, points, count, XF_TOUCH_MAX_POINTS);
        }
        if (err == ESP_OK) {
            return ESP_OK;
        }
        if (attempt < XF_TOUCH_READ_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(XF_TOUCH_RETRY_DELAY_MS));
        }
    }

    xf_bsp_touch_note_error();

    const int64_t now_us = esp_timer_get_time();
    if (s_last_error_log_us == 0 ||
            (now_us - s_last_error_log_us) >= (int64_t)XF_TOUCH_ERROR_LOG_INTERVAL_MS * 1000) {
        s_last_error_log_us = now_us;
        ESP_LOGW(TAG, "touch read failed: %s (total %lu, further logs rate limited)", esp_err_to_name(err),
                 (unsigned long)xf_bsp_touch_error_count());
    }

    return err;
}

/**
 * @brief LVGL read callback; runs in the LVGL task context.
 *
 * Coordinates are taken from the first reported point only. The showcase does
 * not use multitouch, and reporting a single point keeps the interaction model
 * (tap, drag-scroll) unambiguous.
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    if (touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_lcd_touch_point_data_t points[XF_TOUCH_MAX_POINTS] = {0};
    uint8_t count = 0;

    if (touch_sample(touch, points, &count) != ESP_OK) {
        /* Do not fabricate a press from a failed read. */
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (count > 0) {
        data->point.x = points[0].x;
        data->point.y = points[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

esp_err_t xf_bsp_touch_open(xf_bsp_t *bsp)
{
    ESP_RETURN_ON_FALSE(bsp != NULL && bsp->i2c_bus != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(bsp->display != NULL, ESP_ERR_INVALID_STATE, TAG, "display must be ready first");

    const esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST9220_CONFIG();
    esp_err_t err = esp_lcd_new_panel_io_i2c_v2(bsp->i2c_bus, &io_config, &bsp->touch_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch IO init failed: %s", esp_err_to_name(err));
        return err;
    }

    const esp_lcd_touch_config_t touch_config = {
        .x_max = XF_PANEL_H_RES,
        .y_max = XF_PANEL_V_RES,
        .rst_gpio_num = GPIO_NUM_NC, /* TP-RST is a PCA9535 bit, see file comment. */
        .int_gpio_num = XF_PIN_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            /* Raw panel coordinates: LVGL applies the display's 180-degree
             * rotation in indev_pointer_proc (x=479-x, y=479-y). Do not mirror
             * again here; taps, scrolling and swipe directions share one map. */
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .user_data = NULL,
    };

    err = esp_lcd_touch_new_i2c_cst9220(bsp->touch_io, &touch_config, &bsp->touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CST9220 init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Input device creation must happen under the LVGL lock. */
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "LVGL lock unavailable");
        return ESP_ERR_TIMEOUT;
    }

    bsp->indev = lv_indev_create();
    if (bsp->indev == NULL) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "LVGL input device creation failed");
        return ESP_ERR_NO_MEM;
    }

    lv_indev_set_type(bsp->indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(bsp->indev, touch_read_cb);
    lv_indev_set_disp(bsp->indev, bsp->display);
    lv_indev_set_user_data(bsp->indev, bsp->touch);
    lv_timer_t *read_timer = lv_indev_get_read_timer(bsp->indev);
    if (read_timer != NULL) {
        lv_timer_set_period(read_timer, XF_TOUCH_POLL_PERIOD_MS);
    }

    lvgl_port_unlock();

    bsp->touch_ready = true;
    ESP_LOGI(TAG, "touch ready: CST9220 at 0x%02X, INT=%d, %u max points", ESP_LCD_TOUCH_IO_I2C_CST9220_ADDRESS,
             XF_PIN_TOUCH_INT, (unsigned)XF_TOUCH_MAX_POINTS);
    return ESP_OK;
}