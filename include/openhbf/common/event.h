#pragma once

#include <any>
#include <cstdint>
#include <stdexcept>
#include <typeinfo>
#include <utility>

#include "openhbf/common/types.h"

namespace openhbf {

// Phases are part of the simulator contract. Their numeric order is the
// visibility order for state committed in the same cycle.
enum class EventPhase : std::uint8_t {
  Reset = 0,
  MediaCommit,
  FtlCommit,
  Ecc,
  Tsv,
  HostLinkComplete,
  CreditReturn,
  ControllerSchedule,
  HostSchedule,
  FinalDelivery,
};

// Identifies a dispatcher registered by OpenHbfSystem. It is deliberately a
// value, rather than an object pointer, so queued events cannot outlive a
// module through a dangling non-owning reference.
class EventHandlerId {
 public:
  constexpr EventHandlerId() noexcept = default;
  explicit constexpr EventHandlerId(std::uint32_t value) noexcept
      : value_(value) {}

  constexpr std::uint32_t value() const noexcept { return value_; }
  friend constexpr bool operator==(EventHandlerId lhs,
                                   EventHandlerId rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

 private:
  std::uint32_t value_ = 0;
};

// A module-defined payload with checked extraction. The common event layer
// stays independent of Host/Controller/FTL/Media headers while a wrong
// dispatcher/payload pairing still fails explicitly instead of type-punning.
class EventPayload {
 public:
  EventPayload() = default;

  template <typename T>
  static EventPayload make(T value) {
    EventPayload payload;
    payload.value_ = std::move(value);
    return payload;
  }

  bool empty() const noexcept { return !value_.has_value(); }
  const std::type_info& type() const noexcept { return value_.type(); }

  template <typename T>
  const T& get() const {
    const auto* value = std::any_cast<T>(&value_);
    if (value == nullptr) {
      throw std::logic_error("event payload type does not match handler");
    }
    return *value;
  }

 private:
  std::any value_;
};

struct EventEnvelope {
  Cycle cycle{};
  EventPhase phase = EventPhase::Reset;
  std::uint64_t sequence = 0;
  EventToken token{};
  Generation generation{};
  EventHandlerId handler{};
};

class Event {
 public:
  Event(EventEnvelope envelope, EventPayload payload);

  const EventEnvelope& envelope() const noexcept { return envelope_; }
  const EventPayload& payload() const noexcept { return payload_; }

 private:
  EventEnvelope envelope_;
  EventPayload payload_;
};

constexpr std::uint8_t event_phase_order(EventPhase phase) noexcept {
  return static_cast<std::uint8_t>(phase);
}

const char* to_string(EventPhase phase) noexcept;

}  // namespace openhbf
