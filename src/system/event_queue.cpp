#include "openhbx/system/event_queue.h"

#include <algorithm>
#include <limits>
#include <tuple>

namespace openhbx {

bool EventQueue::Later::operator()(const Event& lhs, const Event& rhs) const {
  return std::tie(lhs.view.due, lhs.view.phase, lhs.view.sequence) >
         std::tie(rhs.view.due, rhs.view.phase, rhs.view.sequence);
}

bool EventQueue::register_handler(HandlerId id, Handler handler) {
  if (id.value() == 0 || !handler) return false;
  return handlers_.emplace(id.value(), std::move(handler)).second;
}

ScheduleCode EventQueue::schedule(EventSpec& spec, Cycle now, EventPhase current_phase) {
  if (spec.due < now) return ScheduleCode::PastCycle;
  if (spec.due == now && spec.phase <= current_phase) return ScheduleCode::ClosedPhase;
  if (handlers_.find(spec.handler.value()) == handlers_.end()) return ScheduleCode::UnknownHandler;
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) return ScheduleCode::SequenceExhausted;
  Event event{{spec.due, spec.phase, next_sequence_++, spec.handler, spec.generation},
              std::move(spec.payload)};
  events_.push(std::move(event));
  ++scheduled_;
  return ScheduleCode::Accepted;
}

std::size_t EventQueue::dispatch_due(Cycle now, EventPhase phase, Generation generation) {
  std::size_t count = 0;
  while (!events_.empty()) {
    const auto& view = events_.top().view;
    if (view.due != now || view.phase != phase) break;
    Event event = events_.top();
    events_.pop();
    if (event.view.generation != generation) {
      ++stale_;
      continue;
    }
    handlers_.at(event.view.handler.value())(std::move(event.payload));
    ++dispatched_;
    ++count;
  }
  return count;
}

void EventQueue::discard_generation(Generation current) {
  std::vector<Event> keep;
  while (!events_.empty()) {
    Event event = events_.top();
    events_.pop();
    if (event.view.generation == current) keep.push_back(std::move(event));
    else ++stale_;
  }
  for (auto& event : keep) events_.push(std::move(event));
}

std::size_t EventQueue::rebase_generation(Generation old_generation,
                                          Generation new_generation,
                                          HandlerId handler,
                                          EventPhase phase) {
  std::vector<Event> keep;
  std::size_t rebased = 0;
  while (!events_.empty()) {
    Event event = events_.top();
    events_.pop();
    if (event.view.generation == old_generation &&
        event.view.handler == handler && event.view.phase == phase) {
      event.view.generation = new_generation;
      ++rebased;
    }
    keep.push_back(std::move(event));
  }
  for (auto& event : keep) events_.push(std::move(event));
  return rebased;
}

EventQueueSnapshot EventQueue::snapshot() const {
  auto copy = events_;
  std::vector<EventView> views;
  while (!copy.empty()) { views.push_back(copy.top().view); copy.pop(); }
  return {events_.size(), scheduled_, dispatched_, stale_, std::move(views)};
}

}  // namespace openhbx
