/**
 * @file blink_example_main.c
 * @brief Application entry point of the XIAO Family 2.16" AMOLED showcase.
 *
 * The historical entry filename is retained and is already registered by the
 * existing main/CMakeLists.txt. Build configuration is not changed here.
 *
 * Nothing here touches the panel or LVGL directly. The board is brought up by
 * `xf_bsp`, the pages and the event loop live in `xf_ui`, and this function only
 * sequences the two and reports what the firmware actually found at run time.
 */

#include <stdio.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_caps.h"
#include "esp_rom_uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "xf_touch_protocol.h"

#include "xf_bsp.h"
#include "xf_ui.h"

static const char *TAG = "app";

/**
 * @brief Keep application output off the display's GPIO43/44.
 *
 * The existing configuration uses UART0 as the primary console and native USB
 * Serial/JTAG as its secondary console. UART0 TX/RX share the panel CS/SCK pins.
 * Reopen the standard output streams on the already registered USB VFS before
 * the BSP takes those pins. No driver installation or sdkconfig edit is needed.
 * Bootloader/startup logs before app_main retain their configured destination.
 */
static esp_err_t xf_console_use_usb(void)
{
#if CONFIG_ESP_CONSOLE_UART && (CONFIG_ESP_CONSOLE_UART_NUM == 0)
#if CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG
    if (freopen("/dev/secondary", "w", stdout) == NULL) {
        return ESP_FAIL;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (freopen("/dev/secondary", "w", stderr) == NULL) {
        return ESP_FAIL;
    }
    setvbuf(stderr, NULL, _IONBF, 0);
    /* Route ROM diagnostics as well; application ESP_LOG uses stdio above. */
    esp_rom_output_set_as_console(ESP_ROM_USB_SERIAL_DEVICE_NUM);
#else
    ESP_LOGE(TAG, "UART0 conflicts with panel CS/SCK; no existing secondary USB console");
    return ESP_ERR_INVALID_STATE;
#endif
#endif
    return ESP_OK;
}


/* A one-slot queue coalesces visual selection changes without putting bus I/O
 * in LVGL callbacks. This task alone owns the external V1 command stream. */
static QueueHandle_t s_board_selection;

static void board_selection_changed(uint32_t products, void *context)
{
    (void)context;
    if (s_board_selection != NULL) {
        (void)xQueueOverwrite(s_board_selection, &products);
    }
}

static esp_err_t board_status(xf_tp_status_t *status)
{
    uint8_t raw[4];
    esp_err_t err = xf_bsp_external_read(XF_TP_STATUS, raw);
    if (err == ESP_OK && !xf_tp_decode_status(raw, status)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

static esp_err_t board_command(const uint8_t *data, size_t length)
{
    uint8_t raw[4];
    esp_err_t err = xf_bsp_external_command(data, length, raw);
    if (err != ESP_OK) {
        return err;
    }
    xf_tp_status_t status;
    if (!xf_tp_decode_status(raw, &status)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (status.result != 0U) {
        static TickType_t last_warning;
        TickType_t now = xTaskGetTickCount();
        if (last_warning == 0U || now - last_warning >= pdMS_TO_TICKS(1000)) {
            ESP_LOGW(TAG, "touch board command 0x%02X rejected: RESULT=%u", data[0], status.result);
            last_warning = now;
        }
        return ESP_ERR_INVALID_STATE;
    }
    /* RESULT=0 means accepted, not proof that physical LEDs emitted light. */
    return ESP_OK;
}

static esp_err_t board_key_snapshot(uint32_t *held)
{
    uint8_t raw[4];
    esp_err_t err = xf_bsp_external_read(XF_TP_KEY_STATE, raw);
    if (err != ESP_OK) {
        return err;
    }
    uint32_t keys = xf_tp_u32(raw);
    if ((keys & ~XF_TP_VALID_KEY_MASK) != 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint32_t products = xf_tp_keys_to_products(keys);
    err = xf_ui_touchboard_snapshot(products);
    if (err == ESP_OK) {
        *held = products;
    }
    return err;
}

static esp_err_t board_sync_lights(uint32_t desired, bool *cleared, uint8_t *cursor, bool *done)
{
    uint8_t command[5];
    *done = false;
    if (desired == 0U) {
        command[0] = XF_TP_CLEAR_SELECTION;
        esp_err_t err = board_command(command, 1);
        *done = err == ESP_OK;
        return err;
    }
    if ((desired & (desired - 1U)) == 0U) {
        uint8_t id = 0;
        while ((desired & (UINT32_C(1) << id)) == 0U) {
            ++id;
        }
        command[0] = XF_TP_SELECT_DEVICE;
        command[1] = xf_tp_product_to_key(id);
        esp_err_t err = board_command(command, 2);
        *done = err == ESP_OK;
        return err;
    }
    /* Multi-selection is rebuilt incrementally. CLEAR_SELECTION never removes
     * the slave's independent local touch feedback. One command per poll keeps
     * the touch/display bus responsive. */
    if (!*cleared) {
        command[0] = XF_TP_CLEAR_SELECTION;
        esp_err_t err = board_command(command, 1);
        if (err == ESP_OK) {
            *cleared = true;
            *cursor = 0;
        }
        return err;
    }
    while (*cursor < XF_PRODUCT_COUNT && (desired & (UINT32_C(1) << *cursor)) == 0U) {
        ++*cursor;
    }
    if (*cursor >= XF_PRODUCT_COUNT) {
        *done = true;
        return ESP_OK;
    }
    command[0] = XF_TP_SET_RGB;
    command[1] = xf_tp_product_to_key(*cursor);
    command[2] = 0x8F; command[3] = 0xC3; command[4] = 0x1F;
    esp_err_t err = board_command(command, 5);
    if (err == ESP_OK) {
        ++*cursor;
        uint32_t remaining = desired >> *cursor;
        *done = remaining == 0U;
    }
    return err;
}

static void touchboard_task(void *argument)
{
    (void)argument;
    esp_err_t err = xf_bsp_external_open(XF_TP_ADDRESS, XF_TP_SPEED_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "external master device setup failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    bool online = false, offline_logged = false, ready_seen = false;
    bool resync = true, release_pending = false, lights_dirty = true, lights_cleared = false;
    bool rgb_fault_logged = false;
    uint8_t light_cursor = 0;
    uint32_t desired = 0, held = 0;
    xf_tp_pending_t pending = {0};
    TickType_t snapshot_at = 0;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        uint32_t latest;
        if (xQueueReceive(s_board_selection, &latest, 0) == pdTRUE) {
            latest &= XF_TP_VALID_PRODUCT_MASK;
            if (latest != desired) {
                desired = latest;
                lights_dirty = true;
                lights_cleared = false;
                light_cursor = 0;
            }
        }
        if (release_pending && xf_ui_touchboard_snapshot(0U) == ESP_OK) {
            release_pending = false;
        }
        if (!online) {
            if (xf_bsp_external_probe() != ESP_OK) {
                if (!offline_logged) {
                    ESP_LOGW(TAG, "external touch board 0x42 unavailable; local UI remains active");
                    offline_logged = true;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                wake = xTaskGetTickCount();
                continue;
            }
            online = true;
            offline_logged = false;
            ready_seen = false;
            resync = true;
            /* Retain an uncertain ACK over a transport outage. Observed
             * READY=0, overflow or a changed event identifies a new context. */
            lights_dirty = true;
            lights_cleared = false;
            ESP_LOGI(TAG, "external touch board detected: master GPIO5/6, slave 0x42, 100 kHz");
        }

        xf_tp_status_t status;
        err = board_status(&status);
        if (err != ESP_OK) {
            goto disconnected;
        }
        if ((status.flags & XF_TP_READY) == 0U) {
            if (ready_seen || held != 0U) {
                release_pending = true;
            }
            held = 0U;
            ready_seen = false;
            pending.delivered = false;
            resync = true;
            lights_dirty = true;
            lights_cleared = false;
            goto next_poll;
        }
        ready_seen = true;
        if ((status.flags & XF_TP_RGB_FAULT) != 0U) {
            if (!rgb_fault_logged) {
                ESP_LOGW(TAG, "touch board reports RGB driver fault; events remain enabled");
                rgb_fault_logged = true;
            }
        } else {
            rgb_fault_logged = false;
        }

        if ((status.flags & XF_TP_OVERFLOW) != 0U) {
            const uint8_t clear = XF_TP_CLEAR_EVENTS;
            err = board_command(&clear, 1);
            if (err != ESP_OK) {
                goto disconnected;
            }
            pending.delivered = false;
            resync = true;
            ESP_LOGW(TAG, "touch board FIFO overflow; recovering current held keys");
        }
        if (resync) {
            err = board_key_snapshot(&held);
            if (err != ESP_OK) {
                goto disconnected;
            }
            resync = false;
            snapshot_at = xTaskGetTickCount();
        }

        bool had_events = false;
        /* A bounded batch yields to the shared CST9220 bus and drains further
         * events on the following poll. UP events are acknowledged as well. */
        for (unsigned budget = 0; budget < 8U; ++budget) {
            if (status.count == 0U) {
                break;
            }
            uint8_t raw[4];
            err = xf_bsp_external_read(XF_TP_EVENT_HEAD, raw);
            if (err != ESP_OK) {
                goto disconnected;
            }
            xf_tp_event_t event;
            if (!xf_tp_decode_event(raw, &event)) {
                /* Malformed wire data is not allowed to invent a model. */
                const uint8_t clear = XF_TP_CLEAR_EVENTS;
                err = board_command(&clear, 1);
                if (err != ESP_OK) {
                    goto disconnected;
                }
                pending.delivered = false;
                resync = true;
                ESP_LOGW(TAG, "malformed touch-board event; requesting state recovery");
                break;
            }
            if (event.action == XF_TP_EMPTY) {
                break;
            }
            had_events = true;
            uint8_t product = xf_tp_key_to_product(event.key);
            uint32_t next_held = event.action == XF_TP_DOWN
                                  ? held | (UINT32_C(1) << product)
                                  : held & ~(UINT32_C(1) << product);
            if (!xf_tp_already_delivered(&pending, &event)) {
                err = xf_ui_touchboard_event(product, event.action == XF_TP_DOWN, next_held);
                if (err != ESP_OK) {
                    break; /* Queue pressure: retain the slave head, do not ACK. */
                }
                held = next_held;
                /* Only a newly delivered DOWN requests sound. An UP, idle
                 * snapshot or retry of an uncertain ACK must stay silent. */
                if (event.action == XF_TP_DOWN) {
                    (void)xf_bsp_audio_beep();
                }
                pending.event = event;
                pending.delivered = true;
            }
            uint8_t ack[3];
            xf_tp_ack_bytes(event.seq, ack);
            err = board_command(ack, sizeof(ack));
            if (err == ESP_ERR_INVALID_STATE) {
                /* A reset or changed head may invalidate an old ACK. V1 has no
                 * boot epoch; READY/NACK and a fresh KEY_STATE are the available
                 * recovery signals. A same-payload reset during a lost ACK
                 * cannot be unambiguously distinguished by V1. */
                pending.delivered = false;
                resync = true;
                break;
            }
            if (err != ESP_OK) {
                /* Preserve the delivered marker over an uncertain ACK.
                 * Re-reading the same sequence must not replay the UI action. */
                goto retry_link;
            }
            pending.delivered = false;
            err = board_status(&status);
            if (err != ESP_OK) {
                goto disconnected;
            }
            if ((status.flags & XF_TP_READY) == 0U || (status.flags & XF_TP_OVERFLOW) != 0U) {
                resync = true;
                break;
            }
        }

        if (!had_events && xTaskGetTickCount() - snapshot_at >= pdMS_TO_TICKS(500)) {
            /* Periodic reconciliation is silent if the held mask is unchanged;
             * it must not keep resetting the six-second inactivity deadline. */
            err = board_key_snapshot(&held);
            if (err != ESP_OK) {
                goto disconnected;
            }
            snapshot_at = xTaskGetTickCount();
        }
        if (!had_events && lights_dirty && !resync && (status.flags & XF_TP_RGB_FAULT) == 0U) {
            bool done;
            err = board_sync_lights(desired, &lights_cleared, &light_cursor, &done);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                goto disconnected;
            }
            if (err == ESP_OK && done) {
                lights_dirty = false;
            }
        }
        goto next_poll;

retry_link:
        /* Keep the board attached and retry the current head before deciding a
         * persistent transport error is a disconnect. */
        vTaskDelay(pdMS_TO_TICKS(50));
        err = board_status(&status);
        if (err == ESP_OK && (status.flags & XF_TP_READY) != 0U) {
            goto next_poll;
        }
disconnected:
        online = false;
        ready_seen = false;
        resync = true;
        release_pending = true;
        held = 0U;
        lights_dirty = true;
        lights_cleared = false;
        ESP_LOGW(TAG, "external touch link interrupted: %s", esp_err_to_name(err));
next_poll:
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(XF_TP_POLL_MS));
    }
}

void app_main(void)
{
    if (xf_console_use_usb() != ESP_OK) {
        /* Do not start a display while its pins are still owned by the console. */
        return;
    }
    ESP_LOGI(TAG, "application console uses USB; GPIO43/44 reserved for panel CS/SCK");
    ESP_LOGI(TAG, "XIAO Family 2.16\" AMOLED showcase starting");

    /* 1. Board bring-up: I2C, expander power sequence, QSPI panel, LVGL port and
     *    the touch controller. The function logs its own unverified items. */
    esp_err_t err = xf_bsp_init();
    if (err != ESP_OK) {
        /* No screen means no product to show: fail loudly instead of drawing
         * into a panel that was never initialised. */
        ESP_LOGE(TAG, "board bring-up failed: %s", esp_err_to_name(err));
        return;
    }

    /* 2. Record flash, heap, DMA and PSRAM figures in the boot log so the run
     *    can be compared with the memory budget of the design note. */
    xf_bsp_log_capabilities();

    /* The sink is registered before UI startup; callbacks only overwrite a
     * one-slot queue and never wait for a bus transaction. */
    s_board_selection = xQueueCreate(1, sizeof(uint32_t));
    if (s_board_selection != NULL) {
        (void)xf_ui_set_selection_sink(board_selection_changed, NULL);
    }
    err = xf_ui_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UI start failed: %s", esp_err_to_name(err));
        return;
    }

    /* Audio is optional. Start after the local UI and before polling the slave
     * so its first newly delivered DOWN can receive immediate feedback. */
    err = xf_bsp_audio_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "speaker feedback unavailable: %s; UI and touch-board control continue",
                 esp_err_to_name(err));
    }

    if (s_board_selection != NULL &&
        xTaskCreate(touchboard_task, "xf_touchboard", 5120, NULL, 2, NULL) != pdPASS) {
        ESP_LOGW(TAG, "external protocol task could not start");
    }
    ESP_LOGI(TAG, "startup complete, carousel running");
}