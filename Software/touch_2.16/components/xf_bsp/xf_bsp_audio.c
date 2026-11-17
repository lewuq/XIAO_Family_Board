/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/**
 * @file xf_bsp_audio.c
 * @brief Asynchronous ES8311 -> NS4150B touch confirmation on mainboard J2.
 *
 * Register startup values and the 4.096 MHz/16 kHz coefficient are adapted from
 * the existing Espressif ES8311 reference in the local sensecap-indicator-ha
 * project (components/bsp/src/codec/es8311.c). Only DAC playback is connected;
 * the ES7243 ADC, microphone, filesystem and network are not used here.
 *
 * Mainboard V1.0 schematic pages 4/5/6/7: codec 7-bit address 0x18, P07 audio
 * rail, P04 PA enable, D11/D12/D13/D15 -> GPIO38/39/40/42. The standard I2S
 * driver supplies MCLK=256*Fs and duplicate L/R samples for this mono codec.
 */
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "xf_bsp_internal.h"
#include "xf_board.h"
#include "xf_feedback_tone.h"

#define XF_AUDIO_QUEUE_DEPTH 8U
#define XF_AUDIO_CHUNK_FRAMES 64U
#define XF_AUDIO_TASK_STACK 4096U
#define XF_AUDIO_TASK_PRIORITY 1U
#define XF_AUDIO_IO_TIMEOUT_MS 20
#define XF_AUDIO_WRITE_TIMEOUT_MS 100

static const char *TAG = "xf_bsp_audio";
static i2c_master_dev_handle_t s_codec;
static i2s_chan_handle_t s_tx;
static QueueHandle_t s_requests;
static xf_pca9535_t *s_expander;
static atomic_bool s_ready;

/** Register transactions share the existing GPIO5/6 master bus with touch. */
static esp_err_t codec_write(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = {reg, value};
    return i2c_master_transmit(s_codec, bytes, sizeof(bytes), XF_AUDIO_IO_TIMEOUT_MS);
}

static esp_err_t pa_enable(bool enabled)
{
    return xf_pca9535_write_pin(s_expander, XF_EXP_AUDIO_PORT, XF_EXP_BIT_AUDIO_PA, enabled);
}

/** Mute/disable analog output before releasing clocks, including failure paths. */
static void audio_quiet(void)
{
    if (s_expander != NULL) {
        (void)pa_enable(false);
    }
    if (s_codec != NULL) {
        (void)codec_write(0x31, 0x60);
    }
}

static esp_err_t codec_start(void)
{
    /* ES8311 slave, Philips I2S, 16-bit data, left DAC channel.
     * The coefficient matches MCLK 4.096 MHz and Fs 16 kHz exactly; do not
     * change the sample rate without reviewing this clock table. */
    static const struct { uint8_t reg, value; } setup[] = {
        {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24},
        {0x04, 0x10}, {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00},
        {0x10, 0x1F}, {0x11, 0x7F}, {0x00, 0x80},
        {0x01, 0x3F}, /* MCLK pin input, normal polarity, codec clocks on. */
        {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
        {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
        {0x09, 0x0C}, /* DAC input: I2S 16-bit, left channel. */
        {0x0A, 0x4C}, /* ADC serial output disabled; no recording path. */
        {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A},
        {0x31, 0x60}, /* Keep DAC muted through analog power-up. */
        {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A},
        {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x48}, {0x45, 0x00},
        {0x32, XF_AUDIO_DAC_VOLUME},
    };
    for (size_t i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        esp_err_t err = codec_write(setup[i].reg, setup[i].value);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ES8311 register 0x%02X failed: %s", setup[i].reg, esp_err_to_name(err));
            return err;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    return codec_write(0x31, 0x00);
}

static esp_err_t audio_write(const int16_t *samples, size_t bytes)
{
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s_tx, samples, bytes, &written, XF_AUDIO_WRITE_TIMEOUT_MS);
    return err == ESP_OK && written != bytes ? ESP_ERR_TIMEOUT : err;
}

static esp_err_t play_beep(void)
{
    int16_t pcm[XF_AUDIO_CHUNK_FRAMES * 2U];
    for (uint32_t first = 0; first < XF_BEEP_FRAMES; first += XF_AUDIO_CHUNK_FRAMES) {
        uint32_t frames = XF_BEEP_FRAMES - first;
        if (frames > XF_AUDIO_CHUNK_FRAMES) {
            frames = XF_AUDIO_CHUNK_FRAMES;
        }
        for (uint32_t n = 0; n < frames; ++n) {
            int16_t sample = xf_feedback_sample(first + n);
            pcm[n * 2U] = sample;
            pcm[n * 2U + 1U] = sample;
        }
        esp_err_t err = audio_write(pcm, frames * 2U * sizeof(pcm[0]));
        if (err != ESP_OK) {
            return err;
        }
    }
    /* Explicit zero tail plus DMA auto-clear prevents stale audio repeating
     * while the worker sleeps. The fade envelope removes waveform clicks. */
    memset(pcm, 0, sizeof(pcm));
    return audio_write(pcm, sizeof(pcm));
}

static void audio_task(void *argument)
{
    (void)argument;
    uint8_t request;
    for (;;) {
        if (xQueueReceive(s_requests, &request, portMAX_DELAY) == pdTRUE) {
            esp_err_t err = play_beep();
            if (err != ESP_OK) {
                atomic_store(&s_ready, false);
                audio_quiet();
                (void)i2s_channel_disable(s_tx);
                ESP_LOGW(TAG, "speaker stopped after write failure: %s; UI remains active",
                         esp_err_to_name(err));
                /* Leave the tiny queue allocated: a concurrent nonblocking
                 * sender can safely finish, and future calls see !s_ready. */
                vTaskDelete(NULL);
                return;
            }
        }
    }
}

esp_err_t xf_bsp_audio_start(void)
{
    const xf_bsp_t *bsp = xf_bsp_get();
    if (bsp == NULL || bsp->i2c_bus == NULL || bsp->expander == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_tx != NULL || s_requests != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_expander = bsp->expander;
    atomic_store(&s_ready, false);
    esp_err_t err;
    bool tx_enabled = false;
    const uint8_t bits[] = {XF_EXP_BIT_AUDIO_PA, XF_EXP_BIT_AUDIO_POWER};
    for (size_t i = 0; i < sizeof(bits); ++i) {
        err = xf_pca9535_write_pin(s_expander, XF_EXP_AUDIO_PORT, bits[i], false);
        if (err != ESP_OK) {
            goto fail;
        }
        err = xf_pca9535_configure_output(s_expander, XF_EXP_AUDIO_PORT, bits[i]);
        if (err != ESP_OK) {
            goto fail;
        }
    }
    err = xf_pca9535_write_pin(s_expander, XF_EXP_AUDIO_PORT, XF_EXP_BIT_AUDIO_POWER, true);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    err = i2c_master_probe(bsp->i2c_bus, XF_ES8311_ADDRESS, XF_AUDIO_IO_TIMEOUT_MS);
    if (err != ESP_OK) {
        goto fail;
    }
    const i2c_device_config_t codec_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = XF_ES8311_ADDRESS,
        .scl_speed_hz = XF_I2C_CLOCK_HZ,
    };
    err = i2c_master_bus_add_device(bsp->i2c_bus, &codec_config, &s_codec);
    if (err != ESP_OK) {
        goto fail;
    }
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4;
    channel.dma_frame_num = 128;
    channel.auto_clear_after_cb = true;
    err = i2s_new_channel(&channel, &s_tx, NULL);
    if (err != ESP_OK) {
        goto fail;
    }
    const i2s_std_config_t format = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(XF_BEEP_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = XF_PIN_I2S_MCLK,
            .bclk = XF_PIN_I2S_BCLK,
            .ws = XF_PIN_I2S_WS,
            .dout = XF_PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    err = i2s_channel_init_std_mode(s_tx, &format);
    if (err != ESP_OK) {
        goto fail;
    }
    err = i2s_channel_enable(s_tx); /* Supply MCLK before codec initialization. */
    if (err != ESP_OK) {
        goto fail;
    }
    tx_enabled = true;
    err = codec_start();
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    err = pa_enable(true);
    if (err != ESP_OK) {
        goto fail;
    }
    s_requests = xQueueCreate(XF_AUDIO_QUEUE_DEPTH, sizeof(uint8_t));
    if (s_requests == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    if (xTaskCreate(audio_task, "xf_audio", XF_AUDIO_TASK_STACK, NULL,
                    XF_AUDIO_TASK_PRIORITY, NULL) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    atomic_store(&s_ready, true);
    ESP_LOGI(TAG, "speaker ready: ES8311 0x18, I2S MCLK/BCLK/WS/DOUT=%d/%d/%d/%d, beep=%uHz/%ums",
             XF_PIN_I2S_MCLK, XF_PIN_I2S_BCLK, XF_PIN_I2S_WS, XF_PIN_I2S_DOUT,
             (unsigned)XF_BEEP_FREQUENCY_HZ, (unsigned)XF_BEEP_DURATION_MS);
    return ESP_OK;

fail:
    audio_quiet();
    if (s_tx != NULL) {
        if (tx_enabled) {
            (void)i2s_channel_disable(s_tx);
        }
        (void)i2s_del_channel(s_tx);
        s_tx = NULL;
    }
    if (s_codec != NULL) {
        (void)i2c_master_bus_rm_device(s_codec);
        s_codec = NULL;
    }
    if (s_requests != NULL) {
        vQueueDelete(s_requests);
        s_requests = NULL;
    }
    (void)xf_pca9535_write_pin(s_expander, XF_EXP_AUDIO_PORT, XF_EXP_BIT_AUDIO_POWER, false);
    return err;
}

esp_err_t xf_bsp_audio_beep(void)
{
    if (!atomic_load(&s_ready) || s_requests == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t request = 1U;
    /* Bounded, nonblocking feedback: a burst may drop sound rather than delay
     * protocol ACKs or touch rendering. No allocation occurs per event. */
    return xQueueSend(s_requests, &request, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
