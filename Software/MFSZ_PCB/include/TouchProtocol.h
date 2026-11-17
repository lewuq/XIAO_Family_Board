#pragma once

#include "XiaoDeviceMap.h"

namespace xiao {

// Hardware-independent protocol state. The caller must lock across every access
// when sharing this object between loop() and Wire's slave callback task.
class TouchProtocol {
 public:
  static constexpr uint8_t kAddress = 0x42;
  static constexpr uint8_t kVersion = 1;
  static constexpr size_t kReplyBytes = 4;
  static constexpr size_t kMaxCommandBytes = 5;
  static constexpr uint8_t kQueueCapacity = 32;

  enum Register : uint8_t { Status = 0x00, EventHead = 0x01, KeyState = 0x02 };
  enum Command : uint8_t {
    SelectDevice = 0x10, SetRgb = 0x11, ClearSelection = 0x12,
    AckEvent = 0x20, ClearEvents = 0x21,
  };
  enum Action : uint8_t { Up = 0x00, Down = 0x01, NoEvent = 0xFF };
  enum Result : uint8_t {
    Ok = 0x00, UnknownCommand = 0x01, BadLength = 0x02, BadParameter = 0x03,
    BadSequence = 0x04, NotReady = 0x05, RgbFault = 0x06,
  };
  enum Flag : uint8_t {
    Ready = 0x01, EventPending = 0x02, Overflow = 0x04,
    HostSelection = 0x08, RgbError = 0x10,
  };
  struct Event {
    uint8_t action;
    uint8_t key;
    uint16_t sequence;
  };

  void setReady(bool ready) { ready_ = ready; }
  void setRgbFault(bool fault) { rgbFault_ = fault; }
  uint32_t touchMask() const { return touchMask_; }
  uint32_t selectedMask() const { return selectedMask_; }
  uint8_t eventCount() const { return count_; }
  Result lastResult() const { return result_; }

  uint8_t flags() const {
    return (ready_ ? Ready : 0) | (count_ ? EventPending : 0) |
           (overflow_ ? Overflow : 0) | (selectedMask_ ? HostSelection : 0) |
           (rgbFault_ ? RgbError : 0);
  }

  // Called once after both devices have completed the same debounce scan.
  // Dropped events do not prevent the current bitmap or touch LEDs updating.
  void updateTouchState(uint32_t mask) {
    if (!ready_) return;
    mask &= kValidKeyMask;
    const uint32_t changed = touchMask_ ^ mask;
    touchMask_ = mask;
    for (const auto& device : kDevices) {
      const uint32_t bit = 1UL << device.key;
      if (changed & bit) enqueue((mask & bit) ? Down : Up, device.key);
    }
  }

  Event headEvent() const {
    return count_ ? events_[head_] : Event{NoEvent, 0xFF, 0};
  }

  // Parsing is bounded, allocation-free, and changes no hardware directly.
  // Empty writes (e.g. address probes) do not alter the command result.
  void receive(const uint8_t* bytes, size_t size) {
    if (!bytes || size == 0) return;
    const uint8_t command = bytes[0];
    if (command <= KeyState) {
      if (size != 1) { result_ = BadLength; return; }
      readRegister_ = command; // Selecting a register preserves RESULT.
      return;
    }

    size_t expected = 0;
    switch (command) {
      case SelectDevice: expected = 2; break;
      case SetRgb: expected = 5; break;
      case ClearSelection: case ClearEvents: expected = 1; break;
      case AckEvent: expected = 3; break;
      default: result_ = UnknownCommand; return; // Includes reserved 0x13.
    }
    if (size != expected) { result_ = BadLength; return; }
    if (!ready_) { result_ = NotReady; return; }

    if (command == AckEvent) {
      const uint16_t sequence = static_cast<uint16_t>(bytes[1]) |
                                (static_cast<uint16_t>(bytes[2]) << 8);
      acknowledge(sequence);
      return;
    }
    if (command == ClearEvents) {
      head_ = 0;
      count_ = 0;
      overflow_ = false;
      lastAckValid_ = false;
      result_ = Ok; // Keep nextSequence_ unchanged across a queue clear.
      return;
    }

    // Validate the entire command before changing any host-selected LEDs.
    int8_t index = -1;
    if (command == SelectDevice || command == SetRgb) {
      index = deviceIndex(bytes[1]);
      if (index < 0) { result_ = BadParameter; return; }
    }
    if (rgbFault_) { result_ = RgbFault; return; }
    if (command == ClearSelection) {
      selectedMask_ = 0; // A held touch still lights its own pixel.
    } else if (command == SelectDevice) {
      selectedMask_ = 1UL << bytes[1];
      hostColors_[index] = kSeeedGreen;
    } else { // SetRgb: preserve all other selected devices.
      const Rgb color = {bytes[2], bytes[3], bytes[4]};
      hostColors_[index] = color;
      const uint32_t bit = 1UL << bytes[1];
      if (color == kOff) selectedMask_ &= ~bit;
      else selectedMask_ |= bit;
    }
    result_ = Ok;
  }

  // Every read starts at byte zero; peeking never removes an event.
  void reply(uint8_t* out) const {
    if (readRegister_ == Status) {
      out[0] = kVersion; out[1] = flags(); out[2] = count_; out[3] = result_;
    } else if (readRegister_ == EventHead) {
      const Event event = headEvent();
      out[0] = event.action; out[1] = event.key;
      out[2] = static_cast<uint8_t>(event.sequence);
      out[3] = static_cast<uint8_t>(event.sequence >> 8);
    } else {
      for (size_t byte = 0; byte < kReplyBytes; ++byte)
        out[byte] = static_cast<uint8_t>(touchMask_ >> (8 * byte));
    }
  }

  // Touch green takes priority; releasing restores any host-selected color.
  // The output array is already in physical DIN-to-DOUT WS2812 order.
  void pixelColors(Rgb* out) const {
    for (size_t index = 0; index < kDeviceCount; ++index) {
      const auto& device = kDevices[index];
      const uint32_t bit = 1UL << device.key;
      out[device.pixel] = (touchMask_ & bit) ? kSeeedGreen :
                          ((selectedMask_ & bit) ? hostColors_[index] : kOff);
    }
  }

 private:
  void enqueue(uint8_t action, uint8_t key) {
    if (count_ == kQueueCapacity) { overflow_ = true; return; }
    const uint8_t tail = static_cast<uint8_t>((head_ + count_) % kQueueCapacity);
    events_[tail] = {action, key, nextSequence_};
    nextSequence_ = static_cast<uint16_t>(nextSequence_ + 1U);
    ++count_;
  }

  void acknowledge(uint16_t sequence) {
    // Retrying a successful ACK must never consume the next event.
    if (lastAckValid_ && sequence == lastAck_) { result_ = Ok; return; }
    if (!count_ || events_[head_].sequence != sequence) {
      result_ = BadSequence;
      return;
    }
    head_ = static_cast<uint8_t>((head_ + 1U) % kQueueCapacity);
    --count_;
    lastAck_ = sequence;
    lastAckValid_ = true;
    result_ = Ok;
  }

  Event events_[kQueueCapacity] = {};
  Rgb hostColors_[kDeviceCount] = {};
  uint32_t touchMask_ = 0;
  uint32_t selectedMask_ = 0;
  uint16_t nextSequence_ = 1;
  uint16_t lastAck_ = 0;
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint8_t readRegister_ = Status;
  Result result_ = Ok;
  bool ready_ = false;
  bool overflow_ = false;
  bool lastAckValid_ = false;
  bool rgbFault_ = false;
};

} // namespace xiao
