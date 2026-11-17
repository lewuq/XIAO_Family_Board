#pragma once

#include <stddef.h>
#include <stdint.h>

namespace xiao {

constexpr size_t kDeviceCount = 18;
constexpr uint32_t kValidKeyMask = 0x0007FDFFUL; // K0..K8 and K10..K18.

struct Rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

constexpr bool operator==(Rgb left, Rgb right) {
  return left.red == right.red && left.green == right.green && left.blue == right.blue;
}
constexpr bool operator!=(Rgb left, Rgb right) { return !(left == right); }
constexpr Rgb kOff = {0, 0, 0};
constexpr Rgb kSeeedGreen = {0x8F, 0xC3, 0x1F};

struct Device {
  uint8_t key;       // Protocol KEY_ID equals the printed K number.
  uint8_t pixel;     // Zero-based physical WS2812 index, starting at DIN.
  const char* name;
};

// Keep K9 absent. Screen ordering must not change these IDs or the S routing.
constexpr Device kDevices[kDeviceCount] = {
    {0, 17, "XIAO SAMD21"},
    {1, 16, "XIAO SAMD21 Plus"},
    {2, 15, "XIAO STM32C5"},
    {3, 14, "XIAO MG24 Sense"},
    {4, 13, "XIAO RP2350"},
    {5, 12, "XIAO RP2040"},
    {6, 11, "XIAO RP2040 Plus"},
    {7, 10, "XIAO RA4M1"},
    {8, 9, "XIAO ESP32C3"},
    {10, 8, "XIAO ESP32C5"},
    {11, 7, "XIAO ESP32C6"},
    {12, 6, "XIAO ESP32S3 Plus"},
    {13, 5, "XIAO ESP32S3 Sense"},
    {14, 4, "XIAO nRF52840 Sense"},
    {15, 3, "XIAO nRF52840 Plus"},
    {16, 2, "XIAO nRF54L15 Sense"},
    {17, 1, "XIAO nRF54LM20A Sense"},
    {18, 0, "XIAO nRF54LM20B Sense"},
};

constexpr int8_t deviceIndex(uint8_t key) {
  for (size_t index = 0; index < kDeviceCount; ++index) {
    if (kDevices[index].key == key) return static_cast<int8_t>(index);
  }
  return -1;
}

constexpr bool validDeviceMap() {
  uint32_t keys = 0;
  uint32_t pixels = 0;
  for (const auto& device : kDevices) {
    if (device.key > 18 || device.key == 9 || device.pixel >= kDeviceCount) return false;
    const uint32_t keyBit = 1UL << device.key;
    const uint32_t pixelBit = 1UL << device.pixel;
    if ((keys & keyBit) || (pixels & pixelBit)) return false;
    keys |= keyBit;
    pixels |= pixelBit;
  }
  return keys == kValidKeyMask && pixels == ((1UL << kDeviceCount) - 1UL);
}
static_assert(validDeviceMap(), "Every connected K and WS2812 must appear exactly once.");

} // namespace xiao
