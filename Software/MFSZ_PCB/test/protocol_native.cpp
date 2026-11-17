#include "TouchProtocol.h"

#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

using Protocol = xiao::TouchProtocol;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
    "Line " + std::to_string(__LINE__) + ": " #condition); } while (false)

void write(Protocol& protocol, std::initializer_list<uint8_t> bytes) {
  protocol.receive(bytes.begin(), bytes.size());
}

std::array<uint8_t, 4> read(Protocol& protocol, uint8_t reg) {
  protocol.receive(&reg, 1);
  std::array<uint8_t, 4> bytes{};
  protocol.reply(bytes.data());
  return bytes;
}

void expectReply(Protocol& protocol, uint8_t reg, std::array<uint8_t, 4> expected) {
  CHECK(read(protocol, reg) == expected);
}

void ack(Protocol& protocol, uint16_t sequence) {
  write(protocol, {Protocol::AckEvent, static_cast<uint8_t>(sequence),
                   static_cast<uint8_t>(sequence >> 8)});
}

xiao::Rgb color(Protocol& protocol, uint8_t key) {
  xiao::Rgb colors[xiao::kDeviceCount];
  protocol.pixelColors(colors);
  return colors[xiao::kDevices[xiao::deviceIndex(key)].pixel];
}

void testDeviceMap() {
  const char* names[] = {
    "XIAO SAMD21", "XIAO SAMD21 Plus", "XIAO STM32C5", "XIAO MG24 Sense",
    "XIAO RP2350", "XIAO RP2040", "XIAO RP2040 Plus", "XIAO RA4M1",
    "XIAO ESP32C3", "XIAO ESP32C5", "XIAO ESP32C6", "XIAO ESP32S3 Plus",
    "XIAO ESP32S3 Sense", "XIAO nRF52840 Sense", "XIAO nRF52840 Plus",
    "XIAO nRF54L15 Sense", "XIAO nRF54LM20A Sense", "XIAO nRF54LM20B Sense",
  };
  CHECK(xiao::validDeviceMap());
  for (size_t index = 0; index < xiao::kDeviceCount; ++index) {
    CHECK(xiao::kDevices[index].key == (index < 9 ? index : index + 1));
    CHECK(xiao::kDevices[index].pixel == 17 - index);
    CHECK(std::strcmp(xiao::kDevices[index].name, names[index]) == 0);
    Protocol protocol;
    protocol.setReady(true);
    write(protocol, {Protocol::SelectDevice, xiao::kDevices[index].key});
    CHECK(protocol.lastResult() == Protocol::Ok);
    xiao::Rgb colors[xiao::kDeviceCount];
    protocol.pixelColors(colors);
    for (size_t pixel = 0; pixel < xiao::kDeviceCount; ++pixel)
      CHECK(colors[pixel] == (pixel == 17 - index ? xiao::kSeeedGreen : xiao::kOff));
  }
  CHECK(xiao::deviceIndex(9) == -1);
  CHECK(xiao::deviceIndex(255) == -1);
}

void testBootAndValidation() {
  Protocol protocol;
  expectReply(protocol, Protocol::Status, {1, 0, 0, 0});
  expectReply(protocol, Protocol::EventHead, {0xFF, 0xFF, 0, 0});
  write(protocol, {Protocol::SelectDevice, 17});
  CHECK(protocol.lastResult() == Protocol::NotReady);
  expectReply(protocol, Protocol::Status, {1, 0, 0, Protocol::NotReady});
  protocol.updateTouchState(1UL << 17);
  expectReply(protocol, Protocol::KeyState, {0, 0, 0, 0});
  protocol.setReady(true);
  for (uint8_t key : {9, 19, 255}) {
    write(protocol, {Protocol::SelectDevice, key});
    CHECK(protocol.lastResult() == Protocol::BadParameter);
    CHECK(protocol.selectedMask() == 0);
  }
  write(protocol, {0x13, 0});
  CHECK(protocol.lastResult() == Protocol::UnknownCommand);
  const uint8_t commands[] = {Protocol::SelectDevice, Protocol::SetRgb,
      Protocol::ClearSelection, Protocol::AckEvent, Protocol::ClearEvents};
  const size_t lengths[] = {2, 5, 1, 3, 1};
  for (size_t command = 0; command < sizeof(commands); ++command) {
    uint8_t bytes[9] = {commands[command], 0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t size = 1; size <= sizeof(bytes); ++size) {
      if (size == lengths[command]) continue;
      protocol.receive(bytes, size);
      CHECK(protocol.lastResult() == Protocol::BadLength);
      CHECK(protocol.selectedMask() == 0 && protocol.eventCount() == 0);
    }
  }
  write(protocol, {0x02, 0});
  CHECK(protocol.lastResult() == Protocol::BadLength);
  protocol.receive(nullptr, 0);
  CHECK(protocol.lastResult() == Protocol::BadLength);
  protocol.setRgbFault(true);
  write(protocol, {Protocol::SelectDevice, 17});
  CHECK(protocol.lastResult() == Protocol::RgbFault);
  CHECK(protocol.flags() & Protocol::RgbError);
  CHECK(protocol.selectedMask() == 0);
}

void testEventsAndAcknowledgement() {
  Protocol protocol;
  protocol.setReady(true);
  protocol.updateTouchState((1UL << 17) | 1UL);
  expectReply(protocol, Protocol::EventHead, {1, 0, 1, 0});
  expectReply(protocol, Protocol::EventHead, {1, 0, 1, 0}); // Non-destructive read.
  expectReply(protocol, Protocol::KeyState, {1, 0, 2, 0});
  CHECK(protocol.eventCount() == 2);
  protocol.updateTouchState((1UL << 17) | 1UL);
  CHECK(protocol.eventCount() == 2); // No repeat event for a held pad.
  ack(protocol, 99);
  CHECK(protocol.lastResult() == Protocol::BadSequence && protocol.eventCount() == 2);
  ack(protocol, 1);
  CHECK(protocol.eventCount() == 1);
  ack(protocol, 1); // Lost ACK response: retry must not remove event 2.
  CHECK(protocol.lastResult() == Protocol::Ok && protocol.eventCount() == 1);
  expectReply(protocol, Protocol::EventHead, {1, 17, 2, 0});
  ack(protocol, 2);
  protocol.updateTouchState(0);
  expectReply(protocol, Protocol::EventHead, {0, 0, 3, 0});
  ack(protocol, 3);
  expectReply(protocol, Protocol::EventHead, {0, 17, 4, 0});
  ack(protocol, 4);
  expectReply(protocol, Protocol::EventHead, {0xFF, 0xFF, 0, 0});
  CHECK(!(protocol.flags() & Protocol::EventPending));
  protocol.updateTouchState((1UL << 9) | (1UL << 25));
  CHECK(protocol.eventCount() == 0 && protocol.touchMask() == 0);
}

void testHybridRgb() {
  Protocol protocol;
  protocol.setReady(true);
  write(protocol, {Protocol::SelectDevice, 0});
  CHECK(color(protocol, 0) == xiao::kSeeedGreen);
  CHECK(protocol.eventCount() == 0); // Remote light writes never fake a touch.
  protocol.updateTouchState(1UL << 17);
  CHECK(color(protocol, 17) == xiao::kSeeedGreen);
  CHECK(color(protocol, 0) == xiao::kSeeedGreen);
  write(protocol, {Protocol::SetRgb, 17, 0, 0, 255});
  CHECK(color(protocol, 17) == xiao::kSeeedGreen); // Touch green has priority.
  protocol.updateTouchState(0);
  CHECK(color(protocol, 17) == (xiao::Rgb{0, 0, 255})); // Release restores host blue.
  CHECK(color(protocol, 0) == xiao::kSeeedGreen);
  write(protocol, {Protocol::SetRgb, 17, 0, 0, 0});
  CHECK(color(protocol, 17) == xiao::kOff && color(protocol, 0) == xiao::kSeeedGreen);
  protocol.updateTouchState(1UL << 17);
  write(protocol, {Protocol::SelectDevice, 8});
  CHECK(color(protocol, 17) == xiao::kSeeedGreen && color(protocol, 8) == xiao::kSeeedGreen);
  CHECK(color(protocol, 0) == xiao::kOff); // SELECT replaces only host selection.
  write(protocol, {Protocol::ClearSelection});
  CHECK(color(protocol, 17) == xiao::kSeeedGreen && color(protocol, 8) == xiao::kOff);
  CHECK(protocol.selectedMask() == 0 && !(protocol.flags() & Protocol::HostSelection));
  protocol.updateTouchState(0);
  CHECK(color(protocol, 17) == xiao::kOff);
}

void testOverflowRecoveryAndSequenceWrap() {
  Protocol protocol;
  protocol.setReady(true);
  for (uint8_t index = 0; index < 32; ++index) protocol.updateTouchState(index % 2 ? 0 : 1);
  CHECK(protocol.eventCount() == 32);
  protocol.updateTouchState(1UL << 17); // Event 33 cannot fit.
  CHECK(protocol.eventCount() == 32 && (protocol.flags() & Protocol::Overflow));
  expectReply(protocol, Protocol::KeyState, {0, 0, 2, 0}); // Current state stays valid.
  expectReply(protocol, Protocol::EventHead, {1, 0, 1, 0});
  ack(protocol, 1);
  protocol.updateTouchState(0); // One free slot: enqueue with sequence 33, not 34.
  for (uint16_t sequence = 2; sequence <= 32; ++sequence) ack(protocol, sequence);
  expectReply(protocol, Protocol::EventHead, {0, 17, 33, 0});
  write(protocol, {Protocol::ClearEvents});
  CHECK(protocol.eventCount() == 0 && !(protocol.flags() & Protocol::Overflow));
  protocol.updateTouchState(1);
  expectReply(protocol, Protocol::EventHead, {1, 0, 34, 0});

  Protocol wrapped;
  wrapped.setReady(true);
  for (uint32_t index = 0; index < 65537; ++index) {
    wrapped.updateTouchState(index % 2 ? 0 : 1);
    const auto event = wrapped.headEvent();
    CHECK(event.action != Protocol::NoEvent);
    CHECK(event.sequence == static_cast<uint16_t>(index + 1));
    ack(wrapped, event.sequence);
    CHECK(wrapped.lastResult() == Protocol::Ok && wrapped.eventCount() == 0);
  }
}

int main() {
  try {
    testDeviceMap();
    testBootAndValidation();
    testEventsAndAcknowledgement();
    testHybridRgb();
    testOverflowRecoveryAndSequenceWrap();
    std::cout << "PASS: mappings, validation, event ACK/retry, hybrid RGB, overflow, and 16-bit wrap.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
