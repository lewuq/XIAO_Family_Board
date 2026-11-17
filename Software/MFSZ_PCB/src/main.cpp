#include <Arduino.h>
#include <Wire.h>
#include <esp32-hal-rmt.h>
#include "TouchProtocol.h"

/*
 * MFSZ PCB touch test: XIAO ESP32-S3 Plus and two TTP229-BSF devices.
 * CH1 TP0..TP8 maps to K0..K8; CH2 TP0..TP8 maps to K10..K18.
 * Print press/release events over native USB, light the onboard user LED while
 * any pad is pressed, and light each corresponding WS2812 in Seeed-style green.
 * D1 drives 18 chained pixels: K18..K10 followed by K8..K0 (no K9).
 * Host I2C slave V1 at 0x42: SDA=D9/GPIO8, SCL=D10/GPIO9.
 * Registers expose status, acknowledged touch events, and the current bitmap.
 * Host-selected RGB and immediate local touch feedback operate together.
 *
 * The BSF touch devices use proprietary two-wire serial, separate from host I2C.
 * TP1 has no option pull-down resistor on this PCB, selecting active-low
 * timing: SCL idles HIGH and its first falling edge starts TP0 output.
 * Reference: Tontek TTP229-BSF V1.1, pages 7-9, figure 3-4-2.
 */
namespace {
// Read all 16 bits per device; only the lowest 9 bits (TP0..TP8) are used.
constexpr uint8_t kChipBits = 16;
constexpr uint8_t kUsedPads = 9;
constexpr uint32_t kPollMs = 20;         // Poll both devices every 20 ms.
constexpr uint32_t kDebounceMs = 25;     // Confirm a state after at least 25 ms.
constexpr uint32_t kRawPeriodMs = 250;   // Raw logging interval when enabled.
constexpr uint32_t kHeartbeatMs = 5000;  // Print an ALIVE message every 5 seconds.
constexpr uint32_t kHalfClockUs = 20;    // Approx. 25 kHz: 20 us LOW + 20 us HIGH.
constexpr uint16_t kUsedMask = (1U << kUsedPads) - 1U;
constexpr uint32_t kAllPadsSeen = (1UL << (2 * kUsedPads)) - 1UL;

// The XIAO is a slave on this external bus; the two BSF devices stay on D2..D5.
constexpr uint8_t kHostI2cAddress = xiao::TouchProtocol::kAddress;
constexpr uint8_t kHostI2cSda = D9;
constexpr uint8_t kHostI2cScl = D10;
constexpr uint32_t kHostI2cFrequencyHz = 100000;
constexpr size_t kHostI2cReplyBytes = xiao::TouchProtocol::kReplyBytes;

// Public packet numbering follows K labels, including the unused bit 9 gap.
constexpr uint32_t touchKeyMask(uint16_t ch1, uint16_t ch2) {
  return (static_cast<uint32_t>(ch1) & kUsedMask) |
         ((static_cast<uint32_t>(ch2) & kUsedMask) << 10);
}
static_assert(touchKeyMask(0, 1U << 7) == 0x00020000UL,
              "CH2 TP7 must be K17 in the host bitmap.");
static_assert(touchKeyMask(1U << 8, 1U) == 0x00000500UL,
              "K8 and K10 must be separate bits around the K9 gap.");
static_assert(touchKeyMask(0xFFFF, 0xFFFF) == 0x0007FDFFUL,
              "Only the 18 connected K labels may be published.");

// The Plus user LED is GPIO21 and active LOW. The charge LED is separate.
constexpr uint8_t kUserLedPin = LED_BUILTIN;
constexpr uint8_t kUserLedOn = LOW;
constexpr uint8_t kUserLedOff = HIGH;

// Device names, KEY_IDs, and the S-shaped pixel routing have one source of truth
// in XiaoDeviceMap.h. Printed WS numbers are 1-based; internal indices are not.
constexpr uint8_t kWs2812Pin = D1;
constexpr uint8_t kWs2812Count = xiao::kDeviceCount;
constexpr uint8_t kWs2812Brightness = 64; // 0..255; default is about 25%.
constexpr uint32_t kRmtFrequencyHz = 20000000; // One RMT tick = 50 ns.
constexpr uint32_t kWs2812WriteTimeoutMs = 20;

// Return a zero-based pixel index, or -1 for an unconnected K label.
constexpr int8_t pixelIndexForKey(uint8_t key) {
  const int8_t index = xiao::deviceIndex(key);
  return index < 0 ? -1 : static_cast<int8_t>(xiao::kDevices[index].pixel);
}

// Catch a wrong board variant or mismatched pin mapping at compile time.
static_assert(D1 == 2 && D2 == 3 && D3 == 4 && D4 == 5 && D5 == 6,
              "Check the selected XIAO ESP32-S3 Plus board pin variant.");
static_assert(LED_BUILTIN == 21, "Check the XIAO ESP32-S3 Plus user LED pin.");
static_assert(D9 == 8 && D10 == 9, "Check the host I2C SDA/SCL pin mapping.");

// Each device tracks its own raw data, debounce state, and test coverage.
struct TouchChannel {
  const char* name;       // Channel name printed over USB.
  uint8_t sdo;            // MCU data input pin.
  uint8_t scl;            // MCU clock output pin.
  uint8_t firstKey;       // PCB K number corresponding to local TP0.
  uint16_t raw = 0xFFFF;  // Electrical levels: bit 0 = TP0; LOW means touched.
  uint16_t candidate = 0; // Latest candidate state: a set bit means touched.
  uint16_t stable = 0;    // Debounced state used for events and the user LED.
  uint16_t seen = 0;      // Pads that have generated a DOWN event this session.
  uint32_t changedAt[kUsedPads] = {}; // Last candidate transition per pad.
};

TouchChannel channels[] = {
    {"CH1", D2, D3, 0},   // SDO1=GPIO3, SCL1=GPIO4: K0..K8.
    {"CH2", D4, D5, 10},  // SDO2=GPIO5, SCL2=GPIO6: K10..K18.
};
portMUX_TYPE clockMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t lastPoll = 0;
uint32_t lastRaw = 0;
uint32_t lastHeartbeat = 0;
uint32_t frames = 0;
bool rawEnabled = false;
bool ws2812Ready = false;
bool pixelFrameValid = false; // Force an all-OFF frame at startup.
xiao::Rgb lastPixelColors[kWs2812Count] = {};
// One symbol per data bit, plus one explicit LOW reset/latch symbol.
rmt_data_t pixelData[kWs2812Count * 24 + 1] = {};
bool hostI2cReady = false;
portMUX_TYPE hostI2cMux = portMUX_INITIALIZER_UNLOCKED;
xiao::TouchProtocol hostProtocol; // Lock hostI2cMux around every access.

// Publish both channels together, so a read never mixes two scan iterations.
void publishHostI2cSnapshot() {
  const uint32_t mask = touchKeyMask(channels[0].stable, channels[1].stable);
  portENTER_CRITICAL(&hostI2cMux);
  hostProtocol.updateTouchState(mask); // Updates the bitmap and queues edges.
  portEXIT_CRITICAL(&hostI2cMux);
}

// Wire callbacks run in a separate task. Copy one complete selected register
// under the lock, then release it before calling Wire.write().
// Keep the callback short: no Serial, touch scanning, or WS2812 work here.
void onHostI2cRequest() {
  uint8_t reply[kHostI2cReplyBytes];
  portENTER_CRITICAL(&hostI2cMux);
  hostProtocol.reply(reply);
  portEXIT_CRITICAL(&hostI2cMux);
  Wire.write(reply, sizeof(reply));
}

// Drain malformed/oversized writes too. The parser checks exact lengths before
// reading any parameters, so only the first five bytes need to be retained.
void onHostI2cReceive(int byteCount) {
  (void)byteCount;
  uint8_t bytes[xiao::TouchProtocol::kMaxCommandBytes] = {};
  size_t count = 0;
  while (Wire.available()) {
    const uint8_t value = static_cast<uint8_t>(Wire.read());
    if (count < sizeof(bytes)) bytes[count] = value;
    ++count;
  }
  portENTER_CRITICAL(&hostI2cMux);
  hostProtocol.receive(bytes, count);
  portEXIT_CRITICAL(&hostI2cMux);
}

void initializeHostI2c() {
  portENTER_CRITICAL(&hostI2cMux);
  hostProtocol.setRgbFault(!ws2812Ready);
  hostProtocol.setReady(true); // Touch settling is complete; scanning can start.
  portEXIT_CRITICAL(&hostI2cMux);
  publishHostI2cSnapshot();
  Wire.onReceive(onHostI2cReceive);
  Wire.onRequest(onHostI2cRequest);
  // Explicit pins avoid Wire's default D4/D5, which are used by touch channel 2.
  hostI2cReady = Wire.begin(kHostI2cAddress, kHostI2cSda, kHostI2cScl,
                           kHostI2cFrequencyHz);
}

// Translate the two debounced touch bitmaps into physical WS2812 chain order.
uint32_t touchedPixelMask() {
  uint32_t mask = 0;
  for (const auto& device : xiao::kDevices) {
    const uint8_t key = device.key;
    const auto& channel = channels[key >= 10 ? 1 : 0];
    const uint8_t pad = key - channel.firstKey;
    if (channel.stable & (1U << pad)) mask |= 1UL << device.pixel;
  }
  return mask;
}

// Apply a global brightness limit while preserving the selected RGB ratio.
constexpr uint8_t scaledColor(uint8_t value) {
  return (static_cast<uint16_t>(value) * kWs2812Brightness + 127U) / 255U;
}

// Render touch OR host selection. Cache complete colors, not just a touch mask,
// so a host color change on an already-selected key is also transmitted.
// Hardware transmission stays outside both touch and protocol critical sections.
void updateWs2812() {
  if (!ws2812Ready) return;
  xiao::Rgb desired[kWs2812Count];
  portENTER_CRITICAL(&hostI2cMux);
  hostProtocol.pixelColors(desired);
  portEXIT_CRITICAL(&hostI2cMux);
  bool changed = !pixelFrameValid;
  for (uint8_t pixel = 0; pixel < kWs2812Count; ++pixel)
    changed = changed || desired[pixel] != lastPixelColors[pixel];
  if (!changed) return;

  size_t symbol = 0;
  for (uint8_t pixel = 0; pixel < kWs2812Count; ++pixel) {
    // Standard WS2812 RGB devices accept bytes in G, R, B order, MSB first.
    const uint8_t grb[] = {
        scaledColor(desired[pixel].green),
        scaledColor(desired[pixel].red),
        scaledColor(desired[pixel].blue),
    };
    for (uint8_t value : grb) {
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const bool one = (value & (0x80U >> bit)) != 0;
        auto& pulse = pixelData[symbol++];
        pulse.level0 = 1;
        pulse.duration0 = one ? 16 : 7; // T1H=800 ns, T0H=350 ns.
        pulse.level1 = 0;
        pulse.duration1 = one ? 9 : 18; // Total bit duration = 1.25 us.
      }
    }
  }
  // 400 us LOW reset supports devices requiring a longer latch interval.
  auto& reset = pixelData[symbol];
  reset.level0 = 0;
  reset.duration0 = 4000; // 200 us.
  reset.level1 = 0;
  reset.duration1 = 4000; // Another 200 us.
  if (!rmtWrite(kWs2812Pin, pixelData, RMT_SYMBOLS_OF(pixelData),
                kWs2812WriteTimeoutMs)) {
    // A timed-out transfer may still reference pixelData. Stop further writes
    // and leave that static buffer intact; touch diagnostics keep running.
    ws2812Ready = false;
    portENTER_CRITICAL(&hostI2cMux);
    hostProtocol.setRgbFault(true);
    portEXIT_CRITICAL(&hostI2cMux);
    Serial.println("[WS2812 ERROR] RMT write failed; pixel updates stopped until reset.");
    return;
  }
  for (uint8_t pixel = 0; pixel < kWs2812Count; ++pixel)
    lastPixelColors[pixel] = desired[pixel];
  pixelFrameValid = true;
}

// Initialize the hardware transmitter and clear all 18 pixels before scanning.
void initializeWs2812() {
  pinMode(kWs2812Pin, OUTPUT);
  digitalWrite(kWs2812Pin, LOW);
  delayMicroseconds(400);
  ws2812Ready = rmtInit(kWs2812Pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1,
                        kRmtFrequencyHz);
  if (ws2812Ready) ws2812Ready = rmtSetEOT(kWs2812Pin, LOW);
  updateWs2812();
}

// Show wiring and commands; send ? to repeat this after missing the boot log.
void printHelp() {
  Serial.println();
  Serial.println("=== MFSZ PCB: touch + RGB + I2C slave protocol V1 ===");
  Serial.println("USB CDC 115200 | 16-bit active-LOW serial polling (NOT I2C)");
  Serial.println("CH1: SDO1=D2/GPIO3  SCL1=D3/GPIO4  TP0..8 -> K0..8");
  Serial.println("CH2: SDO2=D4/GPIO5  SCL2=D5/GPIO6  TP0..8 -> K10..18");
  Serial.println("K9 is intentionally absent; TP9..15 have no display pads.");
  Serial.println("User LED: GPIO21 active-LOW; ON while any debounced pad is touched.");
  Serial.printf("WS2812: D1/GPIO2, 18 pixels, GRB, RGB=#8FC31F, brightness=%u/255, ready=%s\n",
                static_cast<unsigned>(kWs2812Brightness), ws2812Ready ? "yes" : "NO");
  Serial.println("WS1..WS18 -> K18,K17,K16,K15,K14,K13,K12,K11,K10,K8,K7,K6,K5,K4,K3,K2,K1,K0");
  Serial.printf("Host I2C: slave=0x%02X SDA=D9/GPIO8 SCL=D10/GPIO9 ready=%s\n",
                static_cast<unsigned>(kHostI2cAddress), hostI2cReady ? "yes" : "NO");
  Serial.println("Master selects register 00=status, 01=event, 02=key bitmap; then reads 4 bytes.");
  Serial.println("Writes: 10 key=select, 11 key R G B=set RGB, 12=clear selection, 20 seqL seqH=ACK, 21=clear events.");
  Serial.println("Touch green is immediate; host selections persist after release. D8 IRQ is not enabled.");
  Serial.println("USB commands: ?=help, s=status, m=device map, r=raw on/off, c=clear coverage");
  Serial.println("Touch each pad for about 1 second, then release it.");
}

// Read TP0 first through TP15 last; preserve raw levels for wiring diagnostics.
uint16_t readFrame(const TouchChannel& channel) {
  uint16_t raw = 0;
  // Leave SCL HIGH between frames. Idle time exceeds the 2 ms serial timeout,
  // restoring TP0 alignment before each frame. Protect only the clock burst
  // from long interrupt gaps; never print over USB inside the critical section.
  portENTER_CRITICAL(&clockMux);
  for (uint8_t bit = 0; bit < kChipBits; ++bit) {
    digitalWrite(channel.scl, LOW);  // Falling edge publishes the current TPn.
    delayMicroseconds(kHalfClockUs);
    if (digitalRead(channel.sdo) == HIGH) {
      raw |= static_cast<uint16_t>(1U << bit);
    }
    digitalWrite(channel.scl, HIGH);
    delayMicroseconds(kHalfClockUs);
  }
  portEXIT_CRITICAL(&clockMux);
  return raw;
}

// Decode active-low data, debounce each pad, and print only DOWN/UP transitions.
void processFrame(TouchChannel& channel, uint16_t raw, uint32_t now) {
  channel.raw = raw;
  // Invert to make pressed bits HIGH; mask out unused TP9..TP15.
  const uint16_t pressed = static_cast<uint16_t>(~raw) & kUsedMask;
  // Independent timers keep one bouncing pad from blocking the other pads.
  for (uint8_t pad = 0; pad < kUsedPads; ++pad) {
    const uint16_t mask = static_cast<uint16_t>(1U << pad);
    const bool value = (pressed & mask) != 0;
    const bool candidate = (channel.candidate & mask) != 0;
    if (value != candidate) {
      // Restart this pad's debounce timer whenever its candidate state changes.
      channel.candidate ^= mask;
      channel.changedAt[pad] = now;
    }
    const bool stable = (channel.stable & mask) != 0;
    if (value != stable && now - channel.changedAt[pad] >= kDebounceMs) {
      // The candidate has persisted long enough: accept the new stable state.
      channel.stable ^= mask;
      if (value) {
        channel.seen |= mask;
      }
      Serial.printf("[%lu ms] %s K%u TP%u %s WS%u raw=0x%04X\n",
                    static_cast<unsigned long>(now), channel.name,
                    static_cast<unsigned>(channel.firstKey + pad),
                    static_cast<unsigned>(pad), value ? "DOWN" : "UP",
                    static_cast<unsigned>(pixelIndexForKey(channel.firstKey + pad) + 1),
                    static_cast<unsigned>(raw));
      Serial.printf("  %s\n", xiao::kDevices[xiao::deviceIndex(channel.firstKey + pad)].name);
    }
  }
}

// Combine both devices: ON while any pad is held, OFF only when all are released.
// Call after processing both channels so releasing one pad cannot turn off the
// LED while another pad on either device is still held.
void updateTouchLed() {
  bool anyTouched = false;
  for (const auto& channel : channels) {
    if (channel.stable != 0) {
      anyTouched = true;
      break;
    }
  }
  digitalWrite(kUserLedPin, anyTouched ? kUserLedOn : kUserLedOff);
}

// Convert a pad bitmask to PCB K labels; the second device starts at K10.
void printKeyList(uint16_t mask, uint8_t firstKey) {
  if (mask == 0) {
    Serial.print("none");
    return;
  }
  bool comma = false;
  for (uint8_t pad = 0; pad < kUsedPads; ++pad) {
    if ((mask & (1U << pad)) != 0) {
      if (comma) Serial.print(',');
      Serial.printf("K%u", static_cast<unsigned>(firstKey + pad));
      comma = true;
    }
  }
}

// Print all 16 bits; unused_active reveals activity on unconnected channels.
void printRaw(const TouchChannel& channel) {
  const uint16_t pressed = static_cast<uint16_t>(~channel.raw);
  Serial.printf("%s raw=0x%04X bits[TP15..TP0]=", channel.name,
                static_cast<unsigned>(channel.raw));
  for (int bit = kChipBits - 1; bit >= 0; --bit) {
    Serial.print((channel.raw & (1U << bit)) ? '1' : '0');
  }
  Serial.print(" active=");
  printKeyList(pressed & kUsedMask, channel.firstKey);
  Serial.printf(" unused_active=0x%04X\n",
                static_cast<unsigned>(pressed & static_cast<uint16_t>(~kUsedMask)));
}

// Report states and coverage. Seeing 18/18 DOWN events is not a hardware PASS.
void printStatus() {
  Serial.printf("\n[STATUS] frames=%lu uptime=%lu ms\n",
                static_cast<unsigned long>(frames),
                static_cast<unsigned long>(millis()));
  uint8_t seenCount = 0;
  uint32_t combinedSeen = 0;
  for (uint8_t index = 0; index < 2; ++index) {
    const auto& channel = channels[index];
    printRaw(channel);
    Serial.printf("%s stable=", channel.name);
    printKeyList(channel.stable, channel.firstKey);
    Serial.print(" | still_to_test=");
    printKeyList(static_cast<uint16_t>(~channel.seen) & kUsedMask, channel.firstKey);
    Serial.println();
    combinedSeen |= static_cast<uint32_t>(channel.seen) << (index * kUsedPads);
    for (uint8_t pad = 0; pad < kUsedPads; ++pad) {
      if (channel.seen & (1U << pad)) ++seenCount;
    }
  }
  Serial.printf("Observed DOWN events: %u/18 pads%s\n",
                static_cast<unsigned>(seenCount),
                combinedSeen == kAllPadsSeen ? " (verify physical labels and UP events too)" : "");
  Serial.printf("WS2812 ready=%s requested=0x%05lX (bit0=WS1; 1=touched)\n",
                ws2812Ready ? "yes" : "NO",
                static_cast<unsigned long>(touchedPixelMask()));
  uint32_t touchMask, selectedMask;
  uint8_t flags, pending, result;
  xiao::TouchProtocol::Event event;
  portENTER_CRITICAL(&hostI2cMux);
  touchMask = hostProtocol.touchMask();
  selectedMask = hostProtocol.selectedMask();
  flags = hostProtocol.flags();
  pending = hostProtocol.eventCount();
  result = hostProtocol.lastResult();
  event = hostProtocol.headEvent();
  portEXIT_CRITICAL(&hostI2cMux);
  Serial.printf("Host I2C slave 0x%02X ready=%s V1 flags=0x%02X pending=%u result=0x%02X\n",
                static_cast<unsigned>(kHostI2cAddress), hostI2cReady ? "yes" : "NO",
                static_cast<unsigned>(flags), static_cast<unsigned>(pending),
                static_cast<unsigned>(result));
  Serial.printf("touch_mask=0x%08lX host_selected=0x%08lX\n",
                static_cast<unsigned long>(touchMask), static_cast<unsigned long>(selectedMask));
  if (event.action != xiao::TouchProtocol::NoEvent)
    Serial.printf("Head: K%u %s seq=%u (read does not pop; master must ACK)\n",
                  static_cast<unsigned>(event.key), event.action ? "DOWN" : "UP",
                  static_cast<unsigned>(event.sequence));
  Serial.println("Raw HIGH=not touched; raw LOW=touched. All 1s can also mean disconnected SDO.");
}

// Print the exact IDs used by both protocol commands and touch event reports.
void printDeviceMap() {
  for (const auto& device : xiao::kDevices)
    Serial.printf("K%u ID=0x%02X WS%u %s\n", static_cast<unsigned>(device.key),
                  static_cast<unsigned>(device.key), static_cast<unsigned>(device.pixel + 1),
                  device.name);
}

// Handle single-character diagnostic commands; no Enter key is required.
void handleSerial() {
  // Bound command work so a continuous USB input stream cannot starve polling.
  for (uint8_t count = 0; count < 16 && Serial.available(); ++count) {
    switch (Serial.read()) {
      case '?': case 'h': case 'H': printHelp(); break;
      case 's': case 'S': printStatus(); break;
      case 'm': case 'M': printDeviceMap(); break;
      case 'r': case 'R':
        rawEnabled = !rawEnabled;
        Serial.printf("Raw output: %s\n", rawEnabled ? "ON" : "OFF");
        break;
      case 'c': case 'C':
        for (auto& channel : channels) channel.seen = 0;
        Serial.println("Coverage cleared. Release all pads before starting the next check.");
        break;
      case '\r': case '\n': case ' ': break;
      default: Serial.println("Unknown command. Send ? for help."); break;
    }
  }
}
}  // namespace

void setup() {
  // Start with the user LED OFF; updateTouchLed later follows debounced touches.
  pinMode(kUserLedPin, OUTPUT);
  digitalWrite(kUserLedPin, kUserLedOff);

  for (auto& channel : channels) {
    // Arduino-ESP32 requires OUTPUT mode before setting the idle HIGH level.
    pinMode(channel.scl, OUTPUT);
    digitalWrite(channel.scl, HIGH);
    pinMode(channel.sdo, INPUT);  // BSF SDO is CMOS push-pull: use plain INPUT.
  }
  Serial.begin(115200);  // Native USB CDC enabled by the selected Plus board.
  initializeWs2812();   // Clear any colors retained by a separately powered chain.
  const uint32_t waitStart = millis();
  // Wait at most 3 seconds for the host; touch scanning also works without USB.
  while (!Serial && millis() - waitStart < 3000) delay(10);
  Serial.println("Settling for 1 second. Keep fingers off all pads.");
  delay(1000);  // Allow 1 second for settling (IC needs about 0.5 s). Do not touch.
  initializeHostI2c();  // Expose the slave address after the settling interval.
  printHelp();
  Serial.println("READY: DOWN=touched, UP=released. Send s to repeat the status.");
  lastPoll = millis() - kPollMs;  // Make the first loop iteration poll immediately.
  lastHeartbeat = millis();
}

void loop() {
  handleSerial();
  const uint32_t now = millis();
  if (now - lastPoll >= kPollMs) {
    lastPoll = now;
    // Read both buses before processing/logging so USB cannot interrupt a frame.
    const uint16_t raw1 = readFrame(channels[0]);
    const uint16_t raw2 = readFrame(channels[1]);
    ++frames;
    processFrame(channels[0], raw1, now);
    processFrame(channels[1], raw2, now);
    publishHostI2cSnapshot(); // Publish debounced K states before refreshing LEDs.
    updateTouchLed();  // Update once using the debounced states of all 18 pads.
  }
  updateWs2812(); // Apply host writes promptly even between touch polling rounds.
  if (rawEnabled && now - lastRaw >= kRawPeriodMs) {
    // Raw logging starts disabled; send r to inspect wiring or sensitivity issues.
    lastRaw = now;
    for (const auto& channel : channels) printRaw(channel);
  }
  if (now - lastHeartbeat >= kHeartbeatMs) {
    // Keep reporting ALIVE even when nobody is touching the PCB.
    lastHeartbeat = now;
    Serial.printf("[ALIVE] frames=%lu; send s=status, r=raw, ?=help\n",
                  static_cast<unsigned long>(frames));
  }
  delay(1);  // Yield to the scheduler instead of spinning continuously.
}
