#include <Arduino.h>
#include <Wire.h>

// Run this sketch on a SECOND board acting as the I2C master.
// Connect its SDA to the touch PCB's D9, SCL to D10, and GND to GND.
// Use 3.3 V pull-ups. Wire.begin() below uses the master board's default pins.
constexpr uint8_t kTouchAddress = 0x42;
constexpr uint8_t kReplyBytes = 4;
uint32_t previousMask = 0;
bool firstReading = true;

void setup() {
  Serial.begin(115200);
  Wire.begin(); // For custom ESP32 master pins, use Wire.begin(sdaGPIO, sclGPIO).
  Wire.setClock(100000);
}

void loop() {
  const size_t received = Wire.requestFrom(kTouchAddress, kReplyBytes);
  if (received != kReplyBytes) {
    while (Wire.available()) Wire.read();
    Serial.println("Touch PCB: missing or incomplete reply from 0x42.");
    delay(500);
    return;
  }

  uint32_t mask = 0;
  for (uint8_t byte = 0; byte < kReplyBytes; ++byte) {
    mask |= static_cast<uint32_t>(static_cast<uint8_t>(Wire.read())) << (8 * byte);
  }
  if (firstReading || mask != previousMask) {
    Serial.print("Touch mask: 0x");
    Serial.println(mask, HEX);
    for (uint8_t key = 0; key <= 18; ++key) {
      if (key == 9) continue;
      const uint32_t bit = 1UL << key;
      if ((mask ^ previousMask) & bit) {
        Serial.print('K');
        Serial.print(key);
        Serial.println((mask & bit) ? " DOWN" : " UP");
      }
    }
    previousMask = mask;
    firstReading = false;
  }
  delay(20); // Poll current states; this interface does not queue past events.
}
