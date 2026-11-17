#include <Arduino.h>
#include <Wire.h>

// Run on a SECOND board. Its SDA connects to the slave's D9, SCL to D10,
// and GND to GND. Wire.begin() uses the MASTER board's default I2C pins.
constexpr uint8_t kAddress = 0x42;
constexpr uint8_t kReplyBytes = 4;
uint16_t lastProcessedSequence = 0;
bool lastProcessedValid = false;
bool online = false;

bool writeCommand(const uint8_t* bytes, size_t size) {
  Wire.beginTransmission(kAddress);
  Wire.write(bytes, size);
  return Wire.endTransmission(true) == 0;
}

bool readRegister(uint8_t reg, uint8_t* out) {
  if (!writeCommand(&reg, 1)) return false;
  if (Wire.requestFrom(kAddress, kReplyBytes) != kReplyBytes) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t byte = 0; byte < kReplyBytes; ++byte)
    out[byte] = static_cast<uint8_t>(Wire.read());
  return true;
}

uint32_t decodeMask(const uint8_t* bytes) {
  uint32_t mask = 0;
  for (uint8_t byte = 0; byte < kReplyBytes; ++byte)
    mask |= static_cast<uint32_t>(bytes[byte]) << (8 * byte);
  return mask;
}

void printState() {
  uint8_t status[4], state[4];
  if (!readRegister(0x00, status) || !readRegister(0x02, state)) {
    Serial.println("Cannot read slave 0x42.");
    return;
  }
  Serial.print("V="); Serial.print(status[0]);
  Serial.print(" flags=0x"); Serial.print(status[1], HEX);
  Serial.print(" events="); Serial.print(status[2]);
  Serial.print(" result=0x"); Serial.print(status[3], HEX);
  Serial.print(" touch_mask=0x"); Serial.println(decodeMask(state), HEX);
}

void sendControl(const uint8_t* bytes, size_t size) {
  if (!writeCommand(bytes, size)) Serial.println("Control write failed.");
  printState(); // RESULT=00 means the command was accepted.
}

void printHelp() {
  Serial.println("I2C MASTER demo for touch SLAVE 0x42:");
  Serial.println("g: select K17 / WS2 green (XIAO nRF54LM20A Sense)");
  Serial.println("b: add K0 / WS18 blue (XIAO SAMD21); preserve other selections");
  Serial.println("x: clear host selections; held touches still light green");
  Serial.println("s: status and key bitmap; ?: help");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // ESP32 custom pins: replace with Wire.begin(sdaGPIO, sclGPIO).
  Wire.setClock(100000);
  printHelp();
}

void loop() {
  for (uint8_t count = 0; count < 16 && Serial.available(); ++count) {
    switch (Serial.read()) {
      case 'g': { const uint8_t cmd[] = {0x10, 0x11}; sendControl(cmd, sizeof(cmd)); break; }
      case 'b': { const uint8_t cmd[] = {0x11, 0x00, 0, 0, 255}; sendControl(cmd, sizeof(cmd)); break; }
      case 'x': { const uint8_t cmd[] = {0x12}; sendControl(cmd, sizeof(cmd)); break; }
      case 's': printState(); break;
      case '?': printHelp(); break;
      default: break;
    }
  }

  uint8_t status[4];
  if (!readRegister(0x00, status)) {
    if (online) Serial.println("Slave offline; reconnect as a new session.");
    online = false;
    lastProcessedValid = false;
    delay(100);
    return;
  }
  online = true;
  if (status[0] != 1 || !(status[1] & 0x01)) {
    lastProcessedValid = false;
    delay(20);
    return;
  }

  if (status[1] & 0x04) { // FIFO overflow: history is incomplete; resync state.
    const uint8_t clear[] = {0x21};
    if (writeCommand(clear, sizeof(clear))) {
      Serial.println("Event overflow: queue cleared; use the current state below.");
      printState();
      lastProcessedValid = false;
    }
    delay(20);
    return;
  }

  // Bounded draining keeps incoming console commands responsive.
  for (uint8_t count = 0; count < 8; ++count) {
    uint8_t event[4];
    if (!readRegister(0x01, event) || event[0] == 0xFF) break;
    if (event[0] > 1 || event[1] > 18 || event[1] == 9) {
      Serial.println("Invalid event packet.");
      break;
    }
    const uint16_t sequence = static_cast<uint16_t>(event[2]) |
                              (static_cast<uint16_t>(event[3]) << 8);
    if (!lastProcessedValid || sequence != lastProcessedSequence) {
      // Put screen/model lookup here. Keep actions idempotent across reconnects.
      Serial.print('K'); Serial.print(event[1]);
      Serial.print(event[0] ? " DOWN seq=" : " UP seq="); Serial.println(sequence);
      lastProcessedSequence = sequence;
      lastProcessedValid = true;
    }
    const uint8_t acknowledge[] = {0x20, event[2], event[3]};
    if (!writeCommand(acknowledge, sizeof(acknowledge))) break;
    if (!readRegister(0x00, status) || status[3] != 0) {
      Serial.println("Event ACK failed; retry the head event next round.");
      break;
    }
    if (status[2] == 0) break;
  }
  delay(20);
}
