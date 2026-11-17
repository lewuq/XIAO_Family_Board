#include <Arduino.h>

/*
 * MFSZ PCB touch test: XIAO ESP32-S3 Plus and two TTP229-BSF devices.
 * CH1 TP0..TP8 maps to K0..K8; CH2 TP0..TP8 maps to K10..K18.
 * Print press/release events over native USB and light the onboard user LED
 * while any connected touch pad is pressed.
 *
 * BSF uses a proprietary two-wire serial interface, not I2C: no address or Wire.
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

// The Plus user LED is GPIO21 and active LOW. The charge LED is separate.
constexpr uint8_t kUserLedPin = LED_BUILTIN;
constexpr uint8_t kUserLedOn = LOW;
constexpr uint8_t kUserLedOff = HIGH;

// Catch a wrong board variant or mismatched pin mapping at compile time.
static_assert(D2 == 3 && D3 == 4 && D4 == 5 && D5 == 6,
              "Check the selected XIAO ESP32-S3 Plus board pin variant.");
static_assert(LED_BUILTIN == 21, "Check the XIAO ESP32-S3 Plus user LED pin.");

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

// Show wiring and commands; send ? to repeat this after missing the boot log.
void printHelp() {
  Serial.println();
  Serial.println("=== MFSZ PCB: two TTP229-BSF touch circuits ===");
  Serial.println("USB CDC 115200 | 16-bit active-LOW serial polling (NOT I2C)");
  Serial.println("CH1: SDO1=D2/GPIO3  SCL1=D3/GPIO4  TP0..8 -> K0..8");
  Serial.println("CH2: SDO2=D4/GPIO5  SCL2=D5/GPIO6  TP0..8 -> K10..18");
  Serial.println("K9 is intentionally absent; TP9..15 have no display pads.");
  Serial.println("User LED: GPIO21 active-LOW; ON while any debounced pad is touched.");
  Serial.println("Commands (no Enter required): ?=help, s=status, r=raw on/off, c=clear coverage");
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
      Serial.printf("[%lu ms] %s K%u TP%u %s raw=0x%04X\n",
                    static_cast<unsigned long>(now), channel.name,
                    static_cast<unsigned>(channel.firstKey + pad),
                    static_cast<unsigned>(pad), value ? "DOWN" : "UP",
                    static_cast<unsigned>(raw));
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
  Serial.println("Raw HIGH=not touched; raw LOW=touched. All 1s can also mean disconnected SDO.");
}

// Handle single-character diagnostic commands; no Enter key is required.
void handleSerial() {
  // Bound command work so a continuous USB input stream cannot starve polling.
  for (uint8_t count = 0; count < 16 && Serial.available(); ++count) {
    switch (Serial.read()) {
      case '?': case 'h': case 'H': printHelp(); break;
      case 's': case 'S': printStatus(); break;
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
  const uint32_t waitStart = millis();
  // Wait at most 3 seconds for the host; touch scanning also works without USB.
  while (!Serial && millis() - waitStart < 3000) delay(10);
  printHelp();
  Serial.println("Settling for 1 second. Keep fingers off all pads.");
  delay(1000);  // Allow 1 second for settling (IC needs about 0.5 s). Do not touch.
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
    updateTouchLed();  // Update once using the debounced states of all 18 pads.
  }
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
