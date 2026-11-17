/**
 * @file xf_board.h
 * @brief Static board description for the XIAO Family 2.16" AMOLED showcase.
 *
 * Responsibility
 * -------------
 * This header is the single place where board wiring, panel geometry and
 * bring-up timings live. Nothing here is a runtime value: the GPIO numbers,
 * expander bit positions and pixel geometry are compile-time constants.
 * Compiling does not validate electrical connectivity: resolve XIAO D-labels
 * to MCU GPIO numbers using the module schematic before assigning bus pins.
 *
 * Provenance of the mapping
 * -------------------------
 * Trace the mainboard U9 signal to its XIAO D-label, then resolve the label
 * using the official XIAO ESP32S3 Plus V1.1 schematic / pin table:
 * https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
 * The existing docs/pinmap.json has incorrect QSPI D0/D1 entries (42/10);
 * the corrected wiring below is authoritative for this BSP. That reference
 * JSON is read-only under the project execution boundary.
 * PCA9535 bit positions are cross-checked against mainboard U12.
 *
 * Timing values below are conservative bring-up values, not vendor-confirmed
 * specifications. The design note lists the module timing data as an open item
 * (HW-02) and the power-rail compatibility as unverified (HW-01). Do not
 * present these constants as verified requirements.
 *
 * No source file may hard-code a GPIO number that is not declared here.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Display --------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/** Panel horizontal resolution in pixels (drawing orientation). */
#define XF_PANEL_H_RES (480)
/** Panel vertical resolution in pixels (drawing orientation). */
#define XF_PANEL_V_RES (480)

/** Startup AMOLED brightness, percent: 0=dark, 100=maximum.
 * Change this macro and rebuild/flash; it does not alter image pixel colours. */
#ifndef XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT
#define XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT 60U
#endif
#if XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT < 0 || XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT > 100
#error "XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT must be between 0 and 100"
#endif


/**
 * @brief Number of pixel lines per LVGL draw buffer.
 *
 * The design note (section 10.3) recommends two strips of 480 x 40 pixels for
 * RGB565, i.e. 480 * 40 * 2 * 2 = 76800 bytes in total. The value is kept in
 * that range because the current `sdkconfig` does not enable PSRAM, so the
 * draw buffers must fit into internal DMA-capable SRAM.
 */
#define XF_PANEL_STRIP_LINES (40)

/** Pixels per LVGL draw buffer (one strip). */
#define XF_PANEL_BUFFER_PIXELS (XF_PANEL_H_RES * XF_PANEL_STRIP_LINES)

/* ------------------------------------------------------------------------- */
/* QSPI (CO5300) ---------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/** QSPI chip select. */
#define XF_PIN_LCD_CS (43)
/** QSPI serial clock. */
#define XF_PIN_LCD_SCK (44)
/**
 * @brief QSPI data wiring resolved through the XIAO ESP32S3 Plus footprint.
 *
 * Mainboard U9: D16 -> QSPI_D0, D19 -> QSPI_D1,
 *               D18 -> QSPI_D2, D17 -> QSPI_D3.
 * Module U2: pad 20/D16 -> GPIO10, pad 21/D19 -> GPIO11,
 *            pad 22/D18 -> GPIO12, pad 23/D17 -> GPIO13.
 * GPIO42 belongs to D15 and does NOT connect to the panel command input.
 */
/** QSPI data 0 / single-line command output: XIAO D16. */
#define XF_PIN_LCD_D0 (10)
/** QSPI data 1: XIAO D19. */
#define XF_PIN_LCD_D1 (11)
/** QSPI data 2: XIAO D18. */
#define XF_PIN_LCD_D2 (12)
/** QSPI data 3: XIAO D17. */
#define XF_PIN_LCD_D3 (13)

/**
 * @brief QSPI pixel clock in Hz.
 *
 * Restore the local CO5300 driver's 40 MHz operating point after bring-up.
 * This is the bus clock, not a measured display frame rate; validate the
 * physical interconnect if artifacts appear at the higher rate.
 */
#define XF_QSPI_CLOCK_HZ (40 * 1000 * 1000)

/** LVGL refresh scheduling in ms; actual frame rate also depends on drawing/DMA. */
#define XF_PANEL_REFRESH_PERIOD_MS (16U)
/** Touch sampling interval in ms, independent of the display refresh timer. */
#define XF_TOUCH_POLL_PERIOD_MS (10U)

/** Large enough for one full strip plus the 4-byte address phase. */
#define XF_QSPI_MAX_TRANSFER_BYTES (XF_PANEL_BUFFER_PIXELS * 2 + 16)

/* ------------------------------------------------------------------------- */
/* I2C (touch controller + I/O expander) ---------------------------------- */
/* ------------------------------------------------------------------------- */

/** I2C data line. */
#define XF_PIN_I2C_SDA (5)
/** I2C clock line. */
#define XF_PIN_I2C_SCL (6)
/** I2C clock in Hz (fast mode). */
#define XF_I2C_CLOCK_HZ (400 * 1000)
/** I2C controller instance number. */
#define XF_I2C_PORT (0)

/** CST9220 touch interrupt line (active low on the module). */
#define XF_PIN_TOUCH_INT (3)
/** PCA9535 interrupt line (active low); not used by the polling path. */
#define XF_PIN_EXPANDER_INT (4)

/**
 * @brief PCA9535 7-bit I2C address.
 *
 * Taken from the schematic reference in the design note (U12, 0x21). Values
 * from `hardware_profile.reference.json` are marked unverified; keep the
 * address here and confirm it on the bus during bring-up.
 */
#define XF_PCA9535_ADDRESS (0x21)

/* ------------------------------------------------------------------------- */
/* PCA9535 port/bit assignments ------------------------------------------- */
/* ------------------------------------------------------------------------- */

/** Expander port 0 drives the two reset lines. */
#define XF_EXP_RESET_PORT (0U)
/** P00 = EP-RST, active low display-controller reset. */
#define XF_EXP_BIT_DISPLAY_RESET (0U)
/** P01 = TP-RST, active low touch-controller reset. */
#define XF_EXP_BIT_TOUCH_RESET (1U)

/** Expander port 1 drives the three power enables. */
#define XF_EXP_POWER_PORT (1U)
/** P10 = EP_VCI_EN, module VCI enable. */
#define XF_EXP_BIT_DISPLAY_MODULE_EN (0U)
/**
 * @brief P11 = EP_VBAT_EN, load-switch enable for the panel VBAT-labelled rail.
 *
 * The name identifies a panel supply, not a mandatory battery-only source.
 * EP_VBAT_IN has board-population-dependent source links (R154/R92). Software
 * controls the enable but cannot measure the populated source or rail voltage.
 * This hardware was reported to work from USB with MUSE firmware; do not infer
 * a missing battery from a black screen or from a completed SPI transaction.
 */
#define XF_EXP_BIT_DISPLAY_VBAT_EN (1U)
/**
 * @brief P16 = VCI_EN, load-switch enable for the panel VCI rail.
 *
 * U23 TPS22916C switches VDD_3V3 to `EP_VCI`, i.e. this rail works on USB power.
 */
#define XF_EXP_BIT_DISPLAY_VCI_EN (6U)

/* ------------------------------------------------------------------------- */
/* Bring-up timing (conservative, unverified against the module datasheet) - */
/* ------------------------------------------------------------------------- */

/** Wait after the last rail is enabled before releasing the resets. */
#define XF_POWER_RAIL_SETTLE_MS (10U)
/** Minimum time both reset lines are held low. */
#define XF_RESET_HOLD_MS (20U)
/** Settle time after a reset is released, before the first command. */
#define XF_RESET_BOOT_MS (120U)

/* ------------------------------------------------------------------------- */
/* Speaker feedback: mainboard U16 ES8311 -> U20 NS4150B -> J2 ------------ */
/* ------------------------------------------------------------------------- */

/** Codec CE is grounded: 7-bit 0x18, not the 8-bit write-address 0x30. */
#define XF_ES8311_ADDRESS (0x18U)
/** U9 D11/D12/D13/D15 resolved with the XIAO ESP32-S3 Plus pin table. */
#define XF_PIN_I2S_MCLK (38)
#define XF_PIN_I2S_BCLK (39)
#define XF_PIN_I2S_WS (40)
#define XF_PIN_I2S_DOUT (42)
/** P07 enables Audio_3V3; P04 enables the NS4150B amplifier (active high). */
#define XF_EXP_AUDIO_PORT (0U)
#define XF_EXP_BIT_AUDIO_POWER (7U)
#define XF_EXP_BIT_AUDIO_PA (4U)
/** DAC digital gain: approximately -15.5 dB; PCM peak is bounded separately. */
#define XF_AUDIO_DAC_VOLUME (0xE0U)

/* ------------------------------------------------------------------------- */
/* Touch ------------------------------------------------------------------ */
/* ------------------------------------------------------------------------- */

/** Maximum number of reported touch points (CST9220 hardware limit). */
#define XF_TOUCH_MAX_POINTS (2U)

/**
 * @brief Number of extra read attempts before a touch sample is dropped.
 *
 * The design note (section 10.4) requires a bounded retry count so a flaky bus
 * cannot stall the UI task and cannot synthesise continuous click events.
 */
#define XF_TOUCH_READ_RETRIES (1U)
/** Delay between touch read attempts, in milliseconds. */
#define XF_TOUCH_RETRY_DELAY_MS (2U)
/** Minimum spacing between two touch-error log lines, in milliseconds. */
#define XF_TOUCH_ERROR_LOG_INTERVAL_MS (1000U)

#ifdef __cplusplus
}
#endif