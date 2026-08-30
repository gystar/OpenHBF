#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <vector>

#include "openhbf/common/event.h"

namespace openhbf {

// The system-owned deterministic event queue. Call advance_to() once for each
// simulated cycle, then pop_ready() in nondecreasing phase order. This makes
// the processed phase frontier explicit and rejects causality violations.
class EventQueue {
 public:
  EventQueue() = default;

  EventToken schedule(Cycle cycle, EventPhase phase, Generation generation,
                      EventHandlerId handler, EventPayload payload = {});

  void advance_to(Cycle cycle);

  // Returns the next event at the current cycle whose phase is no later than
  // through_phase. An empty result closes that phase frontier: subsequent
  // same-cycle scheduling must target a strictly later phase.
  std::optional<Event> pop_ready(EventPhase through_phase);
  std::size_t discard_generation(Generation generation);

  bool empty() const noexcept { return events_.empty(); }
  std::size_t size() const noexcept { return events_.size(); }
  Cycle current_cycle() const noexcept { return current_cycle_; }

 private:
  struct LaterEvent {
    bool operator()(const Event& lhs, const Event& rhs) const noexcept;
  };

  std::priority_queue<Event, std::vector<Event>, LaterEvent> events_;
  Cycle current_cycle_{};
  std::optional<EventPhase> closed_phase_;
  std::uint64_t next_sequence_ = 1;
};

}  // namespace openhbf
