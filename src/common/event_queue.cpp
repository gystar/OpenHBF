#include "openhbf/common/event_queue.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace openhbf {

bool EventQueue::LaterEvent::operator()(const Event& lhs,
                                        const Event& rhs) const noexcept {
  const auto& left = lhs.envelope();
  const auto& right = rhs.envelope();
  if (left.cycle.value() != right.cycle.value()) {
    return left.cycle.value() > right.cycle.value();
  }
  if (left.phase != right.phase) {
    return event_phase_order(left.phase) > event_phase_order(right.phase);
  }
  return left.sequence > right.sequence;
}

EventToken EventQueue::schedule(Cycle cycle, EventPhase phase,
                                Generation generation,
                                EventHandlerId handler,
                                EventPayload payload) {
  if (event_phase_order(phase) > event_phase_order(EventPhase::FinalDelivery)) {
    throw std::invalid_argument("invalid event phase");
  }
  if (handler.value() == 0) {
    throw std::invalid_argument("event handler ID must be non-zero");
  }
  if (cycle.value() < current_cycle_.value()) {
    throw std::logic_error("cannot schedule an event in a past cycle");
  }
  if (cycle.value() == current_cycle_.value() && closed_phase_.has_value() &&
      event_phase_order(phase) <= event_phase_order(*closed_phase_)) {
    throw std::logic_error("cannot schedule into a closed event phase");
  }
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("event sequence space exhausted");
  }

  const std::uint64_t sequence = next_sequence_++;
  const EventToken token(sequence);
  events_.emplace(EventEnvelope{cycle, phase, sequence, token, generation,
                                handler},
                  std::move(payload));
  return token;
}

void EventQueue::advance_to(Cycle cycle) {
  if (cycle.value() < current_cycle_.value()) {
    throw std::logic_error("event queue cycle cannot move backwards");
  }
  if (cycle.value() == current_cycle_.value()) {
    return;
  }
  if (!events_.empty() &&
      events_.top().envelope().cycle.value() < cycle.value()) {
    throw std::logic_error("cannot advance past an unprocessed event");
  }
  current_cycle_ = cycle;
  closed_phase_.reset();
}

std::optional<Event> EventQueue::pop_ready(EventPhase through_phase) {
  if (closed_phase_.has_value() &&
      event_phase_order(through_phase) < event_phase_order(*closed_phase_)) {
    throw std::logic_error("event phase frontier cannot move backwards");
  }

  if (!events_.empty()) {
    const auto& envelope = events_.top().envelope();
    if (envelope.cycle.value() < current_cycle_.value()) {
      throw std::logic_error("event queue contains a missed event");
    }
    if (envelope.cycle.value() == current_cycle_.value() &&
        event_phase_order(envelope.phase) <= event_phase_order(through_phase)) {
      const EventPhase dispatched_phase = envelope.phase;
      Event event = events_.top();
      events_.pop();
      // Close the dispatched event's phase before returning. The handler may
      // enqueue work in a later phase, but cannot recursively refill its own.
      closed_phase_ = dispatched_phase;
      return event;
    }
  }

  closed_phase_ = through_phase;
  return std::nullopt;
}

std::size_t EventQueue::discard_generation(Generation generation) {
  std::vector<Event> kept;
  std::size_t discarded = 0;
  while (!events_.empty()) {
    Event event = events_.top();
    events_.pop();
    if (event.envelope().generation == generation) {
      ++discarded;
    } else {
      kept.push_back(std::move(event));
    }
  }
  for (auto& event : kept) events_.push(std::move(event));
  return discarded;
}

}  // namespace openhbf
