# XIAO Family AMOLED Console

[English](README.md) | [简体中文](README_CN.md)

An embedded product explorer for the XIAO Family, running on the XIAO ESP32-S3 Plus with a 2.16-inch, 480 × 480 rounded-square touch AMOLED. It presents images, specifications and applications for 18 XIAO models, supports side-by-side comparison, and acts as the I²C master for the external touch/WS2812 exhibition board.

The display uses CO5300 over QSPI; CST9220 provides touch input. Product content and image resources are embedded in the firmware. The catalogue works without the external touch board.

## Implemented features

| Feature | Current behavior |
| --- | --- |
| Attract carousel | Displays all 18 models in catalogue order, one image every 3 seconds |
| Display orientation | Software rotation by 180 degrees; LVGL transforms the raw touch coordinates once |
| Touch sound | A new external DOWN event queues one 600 Hz / 40 ms beep; UP, snapshots, carousel changes and ACK retries stay silent |
| Manual navigation | Swipe left for the next model; swipe right for the previous model; wraps at both ends |
| Carousel indicator | Three rolling dots below the image, plus a product counter; no progress bar or Previous/Next buttons |
| Product browsing | Tap the home image/background to open details, or use Browse for the manufacturer-filtered catalogue; Select products is a green filled primary button with an outline and pressed feedback |
| Detail typography and position | Fonts enlarged by approximately 30% (18 -> 23 px, 22 -> 29 px); opening another product starts at the top |
| Detailed information | Images, product features, compute/storage, connectivity, sensors, power/size, applications and data notes |
| Comparison | Select 2–3 models, switch specification categories, and optionally show differences only |
| Language | English at boot; 中文 / EN toggle on home and page headers; current model, selection and scroll position are retained |
| Idle recovery | After 6 seconds without touch, returns to the carousel and advances once; subsequent changes are 3 seconds apart |
| External interaction | Reads copper-pad touch events from the slave and sends host-selection LED commands |
| Degraded operation | Missing external board does not block the local UI; touch initialization failure is logged while display rendering continues |

A held screen touch or external pad prevents idle recovery. Idle recovery clears the comparison selection, filter and visitor navigation state, while retaining the chosen language. Language is held in RAM; reboot restores English.

The interface uses Seeed green `#8FC31F`, deep blue `#004966`, the supplied Seeed logo asset, and embedded English/Chinese fonts. The physical framebuffer remains 480 × 480; controls use a 24-pixel inset with a 432 × 432 logical root. The inset is a layout margin, not a measured glass corner radius.

## Supported models and fixed board mapping

Product IDs define the catalogue order. KEY_ID is the external board's K number; there is no K9. WS numbers are the external slave's LED-chain mapping, not a direct LED driver in this firmware.

| Product ID | Displayed model | MCU manufacturer | KEY_ID (hex) | WS number |
| --- | --- | --- | --- | --- |
| 0 | XIAO SAMD21 | Microchip | 0x00 | 18 |
| 1 | XIAO SAMD21 Plus | Microchip | 0x01 | 17 |
| 2 | XIAO STM32C5 | STMicroelectronics | 0x02 | 16 |
| 3 | XIAO MG24 Sense | Silicon Labs | 0x03 | 15 |
| 4 | XIAO RP2350 | Raspberry Pi | 0x04 | 14 |
| 5 | XIAO RP2040 | Raspberry Pi | 0x05 | 13 |
| 6 | XIAO RP2040 Plus | Raspberry Pi | 0x06 | 12 |
| 7 | XIAO RA4M1 | Renesas | 0x07 | 11 |
| 8 | XIAO ESP32-C3 | Espressif | 0x08 | 10 |
| 9 | XIAO ESP32-C5 | Espressif | 0x0A | 9 |
| 10 | XIAO ESP32-C6 | Espressif | 0x0B | 8 |
| 11 | XIAO ESP32-S3 Plus | Espressif | 0x0C | 7 |
| 12 | XIAO ESP32-S3 Sense | Espressif | 0x0D | 6 |
| 13 | XIAO nRF52840 Sense | Nordic Semiconductor | 0x0E | 5 |
| 14 | XIAO nRF52840 Plus | Nordic Semiconductor | 0x0F | 4 |
| 15 | XIAO nRF54L15 Sense | Nordic Semiconductor | 0x10 | 3 |
| 16 | XIAO nRF54LM20A Sense | Nordic Semiconductor | 0x11 | 2 |
| 17 | XIAO nRF54LM20B Sense | Nordic Semiconductor | 0x12 | 1 |

Manufacturer filtering covers seven chip vendors. These labels identify MCU manufacturers; Seeed Studio is the XIAO board manufacturer.

The LM20B entry currently uses the LM20A reference image and is labelled as a reference in the detail page. Its product/asset IDs remain separate so the image can be replaced later. Unverified or conflicting specifications display “Not verified”; the catalogue distinguishes board-level values from SoC capabilities.

## Hardware interfaces

Wiring constants are maintained in [xf_board.h](components/xf_bsp/include/xf_board.h). Extended D-labels are resolved using the [official XIAO ESP32-S3 Plus pin table](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/#xiao-esp32-s3-plus-front).

| Signal | Screen-board ESP32-S3 GPIO / assignment |
| --- | --- |
| AMOLED QSPI CS / SCK | GPIO43 / GPIO44 |
| AMOLED QSPI D0 / D1 / D2 / D3 | GPIO10 / GPIO11 / GPIO12 / GPIO13 |
| Shared I²C SDA / SCL | GPIO5 / GPIO6 |
| CST9220 touch interrupt | GPIO3 |
| Speaker I2S MCLK / BCLK / WS / DOUT | GPIO38 / GPIO39 / GPIO40 / GPIO42 |
| Audio rail / power-amplifier enable | PCA9535 P07 / P04 |
| PCA9535 interrupt | GPIO4; not used by the current polling path |
| Display / touch reset | PCA9535 P00 / P01 |
| Module VCI / VBAT-labelled rail / VCI enables | PCA9535 P10 / P11 / P16 |

| Device | Interface | 7-bit address / configured rate |
| --- | --- | --- |
| CO5300 AMOLED | QSPI, RGB565 | 40 MHz bus clock |
| CST9220 touch | Shared I²C master bus | `0x5A`, 400 kHz |
| PCA9535 expander | Shared I²C master bus | `0x21`, 400 kHz |
| ES8311 audio codec -> NS4150B amplifier -> J2 speaker | I2C control + standard I2S playback | `0x18`, 400 kHz |
| External touch board | Same I²C master bus | `0x42`, 100 kHz |

The screen board is the master and has no I²C slave address. On connector J3, pin 6 is SDA, pin 7 is SCL, and pin 5 is GND. Connect these to the external slave's SDA, SCL and GND using 3.3 V logic and the intended pull-ups. The protocol's GPIO8/GPIO9 describe the **external slave**, not this screen board.

Application logs use native USB Serial/JTAG to release GPIO43/44 from UART0 for the display. Bootloader/startup logs retain their existing console configuration.

The current configuration targets ESP32-S3 with 16 MB flash; PSRAM is not enabled. Rendering uses two internal DMA-capable 480 × 40 RGB565 strips (76,800 bytes total), partial updates, a 16 ms display refresh timer and 10 ms touch sampling. Software rotation adds one 480 × 40 RGB565 strip (38,400 bytes), for 115,200 bytes of draw/rotation buffers in total. These are configuration values, not a measured frame-rate guarantee.

## External I²C behavior

The complete local contract is `XIAO_Touch_I2C_Protocol_v1.md`; its path is listed below. Protocol helpers live in [xf_touch_protocol.h](components/xf_core/include/xf_touch_protocol.h).

- Poll STATUS every 20 ms while connected. Event processing is bounded to eight events per iteration, followed by another poll.
- Read registers by writing the pointer with STOP, then reading all four bytes in one transaction with STOP. Do not substitute a repeated-start combined read or four separate byte reads.
- Decode both DOWN and UP, deliver the event to the UI queue, then acknowledge its 16-bit little-endian sequence.
- Retain the delivery marker while an ACK is uncertain to avoid replaying the same UI action.
- On FIFO overflow, clear the queue and recover currently held keys from KEY_STATE. Lost historical ordering is not reconstructed.
- Reconcile held keys periodically; unchanged snapshots do not restart the idle timer.
- Retry an unavailable board approximately once per second. Reconnection restores current host selection.

| Screen state | Requested slave LED feedback |
| --- | --- |
| Automatic carousel | LED-follow disabled by default; clear the prior host selection on entry, then keep it clear |
| Manual carousel selection or product detail | Select the displayed model; green `#8FC31F` |
| Catalogue / selection / comparison with selected models | Highlight the selected set; 2–3 choices can light simultaneously |
| Catalogue with no selection, or Clear all | Clear host-selected LEDs |
| Slave pad touch | Slave provides immediate local green feedback; DOWN opens the matching detail page |

Single selection uses `SELECT_DEVICE` (`0x10`). Multiple selection uses `CLEAR_SELECTION` (`0x12`) followed by `SET_RGB` (`0x11`) for each selected model. Local slave touch feedback remains independent and has priority while held. For example, selecting ESP32-S3 Sense sends `10 0D` to `0x42`, selecting K13 / WS6.

This project implements the screen/master side. The external board requires compatible V1 slave firmware. LED selection does not run a functional demo on each displayed XIAO. STATUS.RESULT=`00` means command accepted, not optically verified LED output. V1 has no boot identifier, so a slave restart with identical pending event data during an uncertain ACK cannot always be distinguished.

Speaker feedback uses the onboard ES8311 at 7-bit address `0x18` and NS4150B amplifier. Connect the speaker across J2's VoP/VoN terminals. The codec receives 16 kHz, 16-bit stereo I2S with identical left/right samples; the generated 600 Hz tone has a 5 ms fade at each end and bounded amplitude. An asynchronous worker plays requested sounds; its eight-entry queue may drop audio during overload to keep protocol processing responsive. Audio initialization failure leaves the screen and external-board control available.

Tone timing and amplitude are in `components/xf_bsp/include/xf_feedback_tone.h`; codec gain and wiring are in `xf_board.h`. Audio is requested only inside the newly delivered DOWN branch in `main/blink_example_main.c`. No sound files, additional downloaded components or recording path are required.

### Default screen brightness

In [xf_board.h](components/xf_bsp/include/xf_board.h):

```c
#define XF_PANEL_DEFAULT_BRIGHTNESS_PERCENT 80U
```

The default startup brightness is 80%. Valid values are 0–100: 0 is dark, 100 is maximum. Change the macro and rebuild/flash to apply it. Brightness uses the CO5300 register command and does not change image pixel values.

### Optional carousel LED-follow

In [xf_config.h](components/xf_core/include/xf_config.h):

```c
#define XF_ENABLE_AUTOPLAY_LED_SYNC 0
```

`0` is the default: boot/idle autoplay clears host-selected LEDs. Later automatic slides do not send selection or RGB updates. Manual product selection, comparison, copper-pad feedback, touch-event polling/ACKs and sound continue to work. The slave's local touch feedback survives CLEAR_SELECTION.

Set the macro to `1` and rebuild/flash to restore automatic carousel LED-follow. The screen's 3-second slideshow and 6-second idle recovery operate in either mode.

## Code structure

```text
main/blink_example_main.c       Active app_main; startup, USB console, external protocol task
components/xf_bsp/             Display, touch, speaker worker, expander, power/reset sequence, I²C transport
components/xf_core/            Pure C reducer, carousel/idle rules, selection and wire helpers
components/xf_data/            Product lookup and generated catalogue/IDs
components/xf_assets/          Embedded image descriptors, logo and generated fonts
components/xf_ui/              LVGL pages, theme, formatting, event controller and translations
main/assets/img/              Embedded hero/thumbnail binary payloads
tools/                        Local data, text, logo, font and image generators
tests/test_xf_core.c           Native state-machine, protocol and localization checks
docs/pinmap.json               Older reference; see corrected GPIO constants in xf_board.h
```

Only `main/blink_example_main.c` is registered as the application entry. The historical Blink filename is retained; `main/app_main.c`, `main/model/` and legacy font files under `main/assets/` are not the active UI implementation.

The UI controller serializes business events through a FreeRTOS queue. `xf_core` decides state transitions without hardware or LVGL dependencies; `xf_ui` builds pages under the LVGL lock. A bounded event burst is reduced before one render commit. Carousel widgets are reused, and same-product detail selection/notice updates preserve the specification body and scroll position. Notice expiry uses a timer-only event; delayed scroll callbacks from another product are rejected. External I²C work runs in a separate task, and selection callbacks only update a one-slot queue. Hardware transactions stay out of widget callbacks.

## Build, flash and monitor

Validated environment: ESP-IDF 5.5.2, target `esp32s3`. The component manifest permits IDF >= 5.4; other versions have not been validated here.

| Dependency | Pinned version |
| --- | --- |
| LVGL | 9.4.0, local override at `managed_components/lvgl__lvgl` |
| esp_lvgl_port | 2.9.0 |
| esp_lcd_co5300 | 2.2.0 |
| esp_lcd_touch | 1.2.1, locked transitive dependency |
| esp_lcd_touch_cst9220 | 0.1.1 |
| cmake_utilities | 0.5.3, locked transitive dependency |

Use an initialized ESP-IDF PowerShell terminal with the existing local tools and complete dependencies:

```powershell
Set-Location 'D:\XIAO_test\MFSZ26\XIAO_Family\Software\touch_2.16'
idf.py --no-ccache build
```

The firmware is generated as `build/touch_2.16.bin`. The existing `partitions.csv` provides a 10 MiB factory application partition. The current implementation embeds resources in that application; it does not require a resource filesystem upload.

When programming is intended, replace COM13 with the board's actual USB port:

```powershell
idf.py -p COM13 flash
idf.py -p COM13 monitor
```

Exit the monitor with Ctrl+]. The commands above do not install the SDK or fetch assets. ESP-IDF's component manager may fetch missing dependencies; stop and inspect incomplete local components before building under a no-download requirement. Do not remove the LVGL local override or run a target reset/menuconfig merely to rebuild this configured project.

## Content and asset maintenance

Normal firmware builds use the existing generated resources; regeneration is optional and requires the local source package and installed conversion tools.

| Content | Authoring source / generator |
| --- | --- |
| Product data and stable IDs | Handoff `product_catalog.json` + `asset_manifest.json`; `tools/gen_xf_data.py` |
| English UI / Chinese translations | Handoff `ui_strings.en.json` + translation tables in `tools/gen_xf_strings.py` |
| Fonts | `tools/gen_xf_fonts.py`; installed `C:\Windows\Fonts\NotoSansSC-VF.ttf` and local `tools/node_modules/lv_font_conv` |
| Brand mark | Supplied SVG through handoff brand assets; `tools/gen_xf_brand.py` |
| Product images | Handoff manifest and converted hero/thumbnail payloads; `tools/convert_assets.py` |

After changing translated copy, regenerate strings **before** fonts to include the required Chinese glyphs:

```powershell
python -B tools/gen_xf_strings.py
python -B tools/gen_xf_fonts.py
```

The font generator requires Node.js on PATH and the existing local converter; it does not install packages. Keep product/asset IDs and KEY_ID mappings stable. Replace images through their asset mapping, retain placeholder labels until authentic images exist, and regenerate C/H resources instead of editing generated tables manually. Legacy generator scripts are not required for the normal build.

## Native verification

From the project root, with an installed GCC available on PATH:

```powershell
New-Item -ItemType Directory -Force build/host_tests | Out-Null
gcc -std=gnu17 -Wall -Wextra -Werror `
  -finput-charset=UTF-8 -fexec-charset=UTF-8 `
  -I components/xf_core/include `
  -I components/xf_bsp/include `
  -I components/xf_data/include `
  -I components/xf_data/generated `
  -I components/xf_ui/generated `
  tests/test_xf_core.c `
  components/xf_core/xf_compare.c `
  components/xf_core/xf_idle.c `
  components/xf_core/xf_state_reducer.c `
  components/xf_data/xf_product_lookup.c `
  components/xf_data/generated/xf_product_catalog.c `
  components/xf_ui/generated/xf_strings.c `
  -o build/host_tests/test_xf_core.exe
& ./build/host_tests/test_xf_core.exe
```

The latest software validation on **2026-11-17** completed:

- 2,945 native checks, zero failures: catalogue contracts, comparison rules, idle/carousel timing, language tables, wire IDs, sequence handling held-key recovery, product-owned detail scrolling, stale-scroll rejection, timer-only notices and the feedback waveform envelope/amplitude/period.
- Full ESP-IDF build and partition-size validation.
- Chinese glyph coverage for the generated 18/22/23/29/32-pixel fonts.

Physical display/touch behavior, frame rate, external-board communication, LED output and speaker output require hardware validation. Panel initialization timings and signal integrity remain board-dependent. Boot color-sweep tests are disabled; startup enters the product UI directly.

## Local project references

| Reference | Local path |
| --- | --- |
| Firmware | `D:\XIAO_test\MFSZ26\XIAO_Family\Software\touch_2.16` |
| Mainboard schematics | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\XIAO-Console-AMOLED-Mainboard` |
| Display specifications | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\Display` |
| Original XIAO SVG images | `D:\XIAO_test\MFSZ26\XIAO_Family\Hardware\picture` |
| Design/data/asset handoff | `C:\Users\seeed\Documents\ChatGPT\MFSZ 2026\屏幕交互设计` |
| External I²C protocol | `C:\Users\seeed\Documents\ChatGPT\MFSZ 2026\协议\XIAO_Touch_I2C_Protocol_v1.md` |

These absolute paths describe the current workstation. Existing generated resources are sufficient for an ordinary build; regeneration requires the corresponding local sources. Project maintenance must remain within the requested scope; dependency downloads, SDK/global configuration changes and device flashing require their own authorization.
