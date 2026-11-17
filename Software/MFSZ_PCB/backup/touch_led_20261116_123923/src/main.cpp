#include <Arduino.h>

// TTP229-BSF is NOT I2C. TP1 without an option resistor selects active-low
// SDO and SCL: clock idle HIGH, D0 starts at the first FALLING edge.
// Official timing: Tontek TTP229-BSF V1.1, pages 7-9, figure 3-4-2.
namespace {
constexpr uint8_t kChipBits = 16;
constexpr uint8_t kUsedPads = 9;  // TP0..TP8 on each IC, not 8 channels.
constexpr uint32_t kPollMs = 20;
constexpr uint32_t kDebounceMs = 25;
constexpr uint32_t kRawPeriodMs = 250;
constexpr uint32_t kHeartbeatMs = 5000;
constexpr uint32_t kHalfClockUs = 20;  // Approximately 25 kHz, within 1..512 kHz.
constexpr uint16_t kUsedMask = (1U << kUsedPads) - 1U;
constexpr uint32_t kAllPadsSeen = (1UL << (2 * kUsedPads)) - 1UL;

static_assert(D2 == 3 && D3 == 4 && D4 == 5 && D5 == 6,
              "Check the selected XIAO ESP32-S3 Plus board pin variant.");

struct TouchChannel {
  const char* name;
  uint8_t sdo;
  uint8_t scl;
  uint8_t firstKey;
  uint16_t raw = 0xFFFF;  // Electrical SDO levels, bit 0 = TP0.
  uint16_t candidate = 0;
  uint16_t stable = 0;
  uint16_t seen = 0;
  uint32_t changedAt[kUsedPads] = {};
};

TouchChannel channels[] = {
    {"CH1", D2, D3, 0},   // SDO1 = GPIO3, SCL1 = GPIO4: K0..K8.
    {"CH2", D4, D5, 10},  // SDO2 = GPIO5, SCL2 = GPIO6: K10..K18.
};
portMUX_TYPE clockMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t lastPoll = 0;
uint32_t lastRaw = 0;
uint32_t lastHeartbeat = 0;
uint32_t frames = 0;
bool rawEnabled = false;

void printHelp() {
  Serial.println();
  Serial.println("=== MFSZ PCB: two TTP229-BSF touch circuits ===");
  Serial.println("USB CDC 115200 | 16-bit active-LOW serial polling (NOT I2C)");
  Serial.println("CH1: SDO1=D2/GPIO3  SCL1=D3/GPIO4  TP0..8 -> K0..8");
  Serial.println("CH2: SDO2=D4/GPIO5  SCL2=D5/GPIO6  TP0..8 -> K10..18");
  Serial.println("K9 is intentionally absent; TP9..15 have no display pads.");
  Serial.println("Commands (no Enter required): ?=help, s=status, r=raw on/off, c=clear coverage");
  Serial.println("Touch each pad for about 1 second, then release it.");
}

uint16_t readFrame(const TouchChannel& channel) {
  uint16_t raw = 0;
  // At least kPollMs idle HIGH between frames on the same IC. This exceeds
  // the 2 ms serial timeout and restores TP0 alignment before every frame.
  // Keep inter-edge gaps bounded while clocking; no USB/printing inside here.
  portENTER_CRITICAL(&clockMux);
  for (uint8_t bit = 0; bit < kChipBits; ++bit) {
    digitalWrite(channel.scl, LOW);  // Active-low leading edge: publish TPn.
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

void processFrame(TouchChannel& channel, uint16_t raw, uint32_t now) {
  channel.raw = raw;
  const uint16_t pressed = static_cast<uint16_t>(~raw) & kUsedMask;
  // Per-pad debounce preserves independent simultaneous touches.
  for (uint8_t pad = 0; pad < kUsedPads; ++pad) {
    const uint16_t mask = static_cast<uint16_t>(1U << pad);
    const bool value = (pressed & mask) != 0;
    const bool candidate = (channel.candidate & mask) != 0;
    if (value != candidate) {
      channel.candidate ^= mask;
      channel.changedAt[pad] = now;
    }
    const bool stable = (channel.stable & mask) != 0;
    if (value != stable && now - channel.changedAt[pad] >= kDebounceMs) {
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

void handleSerial() {
  // Bound command work per loop so an incoming stream cannot starve polling.
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
  for (auto& channel : channels) {
    // Arduino-ESP32 requires OUTPUT mode before digitalWrite can set the pin.
    // Establish idle HIGH before the one-second settling interval.
    pinMode(channel.scl, OUTPUT);
    digitalWrite(channel.scl, HIGH);
    pinMode(channel.sdo, INPUT);  // BSF serial SDO is CMOS push-pull.
  }
  Serial.begin(115200);  // Native USB CDC from the selected Plus board variant.
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 3000) delay(10);
  printHelp();
  Serial.println("Settling for 1 second. Keep fingers off all pads.");
  delay(1000);  // TTP229 power-on stabilization is approximately 0.5 seconds.
  Serial.println("READY: DOWN=touched, UP=released. Send s to repeat the status.");
  lastPoll = millis() - kPollMs;
  lastHeartbeat = millis();
}

void loop() {
  handleSerial();
  const uint32_t now = millis();
  if (now - lastPoll >= kPollMs) {
    lastPoll = now;
    // Read both buses first. Logging cannot interrupt a frame or clock the other IC.
    const uint16_t raw1 = readFrame(channels[0]);
    const uint16_t raw2 = readFrame(channels[1]);
    ++frames;
    processFrame(channels[0], raw1, now);
    processFrame(channels[1], raw2, now);
  }
  if (rawEnabled && now - lastRaw >= kRawPeriodMs) {
    lastRaw = now;
    for (const auto& channel : channels) printRaw(channel);
  }
  if (now - lastHeartbeat >= kHeartbeatMs) {
    lastHeartbeat = now;
    Serial.printf("[ALIVE] frames=%lu; send s=status, r=raw, ?=help\n",
                  static_cast<unsigned long>(frames));
  }
  delay(1);
}
