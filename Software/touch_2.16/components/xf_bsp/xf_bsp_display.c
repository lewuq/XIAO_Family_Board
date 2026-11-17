/**
 * @file xf_bsp_display.c
 * @brief QSPI + CO5300 + `esp_lvgl_port` display bring-up.
 *
 * Interface choice
 * ----------------
 * The module is wired for quad SPI (design note section 3.1/3.2): four data
 * lines, no D/C line. That dictates the whole QSPI configuration below:
 *
 * - `dc_gpio_num = -1`, because command and parameter phases are encoded in the
 *   opcode that the CO5300 component builds (0x02 write command, 0x32 write
 *   colour); a D/C pin does not exist on the flex.
 * - `lcd_cmd_bits = 32`, because the 32-bit opcode carries the opcode byte plus
 *   the 24-bit address/command field.
 * - `flags.quad_mode`, so pixel payloads use four lines. The IDF SPI panel IO
 *   sends the 32-bit command and register parameters over D0 (1-1-4 framing).
 * - `vendor_config.flags.use_qspi_interface`, so the component itself prefixes
 *   commands with the QSPI write opcodes instead of the plain SPI ones.
 *
 * Reset handling
 * --------------
 * The panel reset is a PCA9535 bit (P00), already pulsed by
 * `xf_bsp_power_open()`. The panel is therefore created with
 * `reset_gpio_num = GPIO_NUM_NC`; the driver then performs a software reset
 * (0x01) followed by its init table, which is the documented fallback path of
 * the CO5300 component.
 *
 * Colour order and byte order are candidate values only. Design note HW-04 marks
 * the physical orientation, RGB/BGR order, RGB565 byte order and touch direction
 * as not yet measured; they are collected in the constants below so a single
 * place changes after a four-colour/four-corner check.
 */

#include "xf_bsp_internal.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "xf_board.h"

static const char *TAG = "xf_bsp_disp";

/** Panel RGB element order. HW-04: must be confirmed on the real module. */
#define XF_PANEL_ELEMENT_ORDER (LCD_RGB_ELEMENT_ORDER_RGB)
/** Hardware colour inversion (CO5300 `INVON`). HW-04: must be confirmed. */
#define XF_PANEL_INVERT_COLOR (false)
/** Mirror/swap of the drawing orientation. HW-04: must be confirmed. */
#define XF_PANEL_SWAP_XY (false)
#define XF_PANEL_MIRROR_X (false)
#define XF_PANEL_MIRROR_Y (false)
/**
 * @brief Byte swap inside RGB565 pixels.
 *
 * LVGL keeps RGB565 little-endian in memory while the CO5300 expects the high
 * byte first on the bus, so the port swaps bytes on flush. HW-04: verify with a
 * known colour patch; a wrong setting shows as red/blue or byte-swapped tint.
 */
#define XF_PANEL_SWAP_BYTES (true)

/**
 * @brief Bus framing used to talk to the panel.
 *
 * `XF_PANEL_IFACE_QSPI` uses a single-line 32-bit command field and register
 * parameters, followed by quad-line pixel payloads. The opcode byte selects
 * "write command" (0x02) or "write colour" (0x32). This is what the CO5300
 * component's own `CO5300_PANEL_IO_QSPI_CONFIG` macro specifies.
 *
 * `XF_PANEL_IFACE_SPI` is the single-line fallback: 8-bit command over D0 only,
 * no quad mode, no QSPI opcode prefix. It exists because the module is only
 * documented as "QSPI/MIPI" (datasheet title) and the bring-up note marks the
 * interface as unverified: flipping this macro tests whether the panel latches
 * commands in single-line mode instead. Nothing else needs to change.
 */
#define XF_PANEL_IFACE_QSPI (1)
#define XF_PANEL_IFACE_SPI (0)
#define XF_PANEL_IFACE (XF_PANEL_IFACE_QSPI)

/** SPI host used by the panel (SPI2 is the general-purpose host on S3). */
#define XF_QSPI_HOST (SPI2_HOST)

/**
 * @brief Explicit 480x480 bring-up sequence for the CO5300 panel.
 *
 * Preserve the local driver's existing control-register values, but remove its
 * unrelated 472x466 address window and 60 ms sleep-out wait. Both axes here use
 * 0..479 and sleep-out waits 120 ms. This is a candidate derived from the local
 * driver, not a manufacturer-validated H0216F002AM tuning/gamma sequence.
 * Keep it in the application so managed components remain unchanged.
 *
 * MADCTL and RGB565 COLMOD are applied by esp_lcd_panel_init() before this list.
 * TE is disabled because this application does not synchronize to a TE input.
 */
static const co5300_lcd_init_cmd_t s_panel_init_480x480[] = {
    {0xFE, (const uint8_t[]){0x00}, 1, 0},    /* Base command page. */
    {0xC4, (const uint8_t[]){0x80}, 1, 0},    /* Retain existing driver setting. */
    {0x34, NULL, 0, 0},                      /* Tearing-effect output off. */
    {0x53, (const uint8_t[]){0x20}, 1, 10},   /* Display brightness control. */
    {0x51, (const uint8_t[]){0xFF}, 1, 10},   /* Visible bring-up brightness. */
    {0x63, (const uint8_t[]){0xFF}, 1, 10},   /* Retain existing driver setting. */
    {0x2A, (const uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0}, /* X: 0..479. */
    {0x2B, (const uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0}, /* Y: 0..479. */
    {0x11, NULL, 0, 120},                    /* Exit sleep; wait before display-on. */
    {0x29, NULL, 0, 20},                     /* Display on; settle before pixels. */
};



esp_err_t xf_bsp_display_open(xf_bsp_t *bsp)
{
    ESP_RETURN_ON_FALSE(bsp != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    /* Explicitly reclaim the former UART0 pins before attaching SPI signals.
     * app_main has already moved application/ROM output to native USB.
     * Keep CS inactive while SCK is remapped to avoid a spurious command. */
    ESP_RETURN_ON_ERROR(gpio_reset_pin(XF_PIN_LCD_CS), TAG, "reset CS pin failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(XF_PIN_LCD_CS, 1), TAG, "preload CS high failed");
    ESP_RETURN_ON_ERROR(gpio_set_direction(XF_PIN_LCD_CS, GPIO_MODE_OUTPUT), TAG, "configure CS failed");
    ESP_RETURN_ON_ERROR(gpio_reset_pin(XF_PIN_LCD_SCK), TAG, "reset SCK pin failed");

    /* --- QSPI bus ------------------------------------------------------- */
    const spi_bus_config_t bus_config = {
        .sclk_io_num = XF_PIN_LCD_SCK,
        .data0_io_num = XF_PIN_LCD_D0,
        .data1_io_num = XF_PIN_LCD_D1,
        .data2_io_num = XF_PIN_LCD_D2,
        .data3_io_num = XF_PIN_LCD_D3,
        .max_transfer_sz = XF_QSPI_MAX_TRANSFER_BYTES,
        /* Master, quad-capable I/O; the panel is write-only on this bus. */
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD,
    };

    esp_err_t err = spi_bus_initialize(XF_QSPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "QSPI bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "QSPI bus ready: CS=%d SCK=%d D0..D3=%d/%d/%d/%d", XF_PIN_LCD_CS, XF_PIN_LCD_SCK, XF_PIN_LCD_D0,
             XF_PIN_LCD_D1, XF_PIN_LCD_D2, XF_PIN_LCD_D3);

    /* --- Panel IO ------------------------------------------------------- */
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = XF_PIN_LCD_CS,
        .dc_gpio_num = -1, /* QSPI: the opcode carries the phase, there is no D/C pin. */
        .spi_mode = 0,
        .pclk_hz = XF_QSPI_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL, /* `esp_lvgl_port` registers its own callback. */
        .user_ctx = NULL,
#if XF_PANEL_IFACE == XF_PANEL_IFACE_QSPI
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = {
            .quad_mode = true,
        },
#else
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .quad_mode = false,
        },
#endif
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)XF_QSPI_HOST, &io_config, &bsp->panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel IO init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Panel --------------------------------------------------------- */
    const co5300_vendor_config_t vendor_config = {
        .init_cmds = s_panel_init_480x480,
        .init_cmds_size = sizeof(s_panel_init_480x480) / sizeof(s_panel_init_480x480[0]),
        .flags = {
            .use_qspi_interface = (XF_PANEL_IFACE == XF_PANEL_IFACE_QSPI) ? 1 : 0,
            .use_mipi_interface = 0,
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC, /* Reset is a PCA9535 bit, see file comment. */
        .rgb_ele_order = XF_PANEL_ELEMENT_ORDER,
        .bits_per_pixel = 16,
        .flags = {
            .reset_active_high = 0,
        },
        .vendor_config = (void *)&vendor_config,
    };

    err = esp_lcd_new_panel_co5300(bsp->panel_io, &panel_config, &bsp->panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CO5300 create failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_lcd_panel_reset(bsp->panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CO5300 reset failed: %s", esp_err_to_name(err));
        return err;
    }

    /* The driver software reset waits 80 ms; allow a full 120 ms before init. */
    vTaskDelay(pdMS_TO_TICKS(40));
    ESP_LOGI(TAG, "CO5300 explicit 480x480 init, sleep-out wait=120 ms, QSPI=%d Hz", XF_QSPI_CLOCK_HZ);
    err = esp_lcd_panel_init(bsp->panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CO5300 init failed: %s", esp_err_to_name(err));
        return err;
    }

    if (XF_PANEL_INVERT_COLOR) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(bsp->panel, true), TAG, "colour inversion failed");
    }

    err = esp_lcd_panel_disp_on_off(bsp->panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CO5300 display-on failed: %s", esp_err_to_name(err));
        return err;
    }

    /* AMOLED brightness is part of bring-up, not a cosmetic post-LVGL step. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_co5300_set_brightness(bsp->panel, XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT),
                        TAG, "boot brightness failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    /* --- LVGL port ------------------------------------------------------ */
    lvgl_port_cfg_t port_config = ESP_LVGL_PORT_INIT_CONFIG();
    port_config.task_max_sleep_ms = XF_PANEL_REFRESH_PERIOD_MS;
    err = lvgl_port_init(&port_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(err));
        return err;
    }

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = bsp->panel_io,
        .panel_handle = bsp->panel,
        .buffer_size = XF_PANEL_BUFFER_PIXELS,
        .double_buffer = true,
        .hres = XF_PANEL_H_RES,
        .vres = XF_PANEL_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = XF_PANEL_SWAP_XY,
            .mirror_x = XF_PANEL_MIRROR_X,
            .mirror_y = XF_PANEL_MIRROR_Y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            /* Internal SRAM: no PSRAM is enabled in the current sdkconfig. */
            /* Rotate rendered strips in software; the panel keeps its native scan. */
            .sw_rotate = true,
            .buff_dma = true,
            .buff_spiram = false,
            .swap_bytes = XF_PANEL_SWAP_BYTES,
            .full_refresh = false,
            .direct_mode = false,
        },
    };

    bsp->display = lvgl_port_add_disp(&display_config);
    if (bsp->display == NULL) {
        ESP_LOGE(TAG, "LVGL display registration failed");
        return ESP_FAIL;
    }

    /* Change the public LVGL refresh timer at runtime; keep sdkconfig and
     * managed components intact. Partial refresh and both DMA strips remain. */
    ESP_RETURN_ON_FALSE(lvgl_port_lock(0), ESP_ERR_TIMEOUT, TAG, "LVGL refresh lock failed");
    /* LVGL also maps raw pointer coordinates through this rotation. The touch
     * driver must keep its mirror/swap flags clear to avoid a double transform.
     * esp_lvgl_port owns the extra strip-sized rotation buffer. */
    lv_display_set_rotation(bsp->display, LV_DISPLAY_ROTATION_180);
    lv_timer_t *refresh_timer = lv_display_get_refr_timer(bsp->display);
    if (refresh_timer != NULL) {
        lv_timer_set_period(refresh_timer, XF_PANEL_REFRESH_PERIOD_MS);
    }
    lvgl_port_unlock();

    bsp->display_ready = true;
    ESP_LOGI(TAG, "display ready: %dx%d, rotation=180 software, %u px per buffer, double buffered, element order %d, interface %s",
             XF_PANEL_H_RES, XF_PANEL_V_RES, (unsigned)XF_PANEL_BUFFER_PIXELS, (int)XF_PANEL_ELEMENT_ORDER,
             XF_PANEL_IFACE == XF_PANEL_IFACE_QSPI ? "qspi(4-bit,32-bit cmd)" : "spi(1-bit,8-bit cmd)");
    return ESP_OK;
}