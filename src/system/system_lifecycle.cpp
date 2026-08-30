#include "openhbx/system/system_lifecycle.h"

#include <limits>
#include <stdexcept>

#include "openhbx/common/checked_math.h"

namespace openhbx {

SystemLifecycle::SystemLifecycle(EventQueue& events, CompletionRegistry& completions)
    : events_(events), completions_(completions) {}

void SystemLifecycle::advance_cycle() {
  auto next = checked_add(cycle_.value(), 1);
  if (!next) throw std::overflow_error("system cycle overflow");
  cycle_ = Cycle(*next);
}

bool SystemLifecycle::reset() {
  if (mode_ != SystemMode::Running) return false;
  mode_ = SystemMode::Resetting;
  const Generation old = generation_;
  auto next = checked_add(generation_.value(), 1);
  if (!next) { mode_ = SystemMode::Running; return false; }
  generation_ = Generation(*next);
  completions_.abort_generation(old);
  events_.discard_generation(generation_);
  mode_ = SystemMode::Running;
  return true;
}

bool SystemLifecycle::reset_without_completion_abort() {
  if (mode_ != SystemMode::Running) return false;
  mode_ = SystemMode::Resetting;
  auto next = checked_add(generation_.value(), 1);
  if (!next) { mode_ = SystemMode::Running; return false; }
  generation_ = Generation(*next);
  events_.discard_generation(generation_);
  mode_ = SystemMode::Running;
  return true;
}

SystemSnapshot SystemLifecycle::snapshot(const std::vector<IdleReason>& external) const {
  std::vector<IdleReason> reasons = external;
  if (!events_.empty()) reasons.push_back({"event_queue", "future events queued"});
  if (!completions_.empty()) reasons.push_back({"completion_registry", "terminal obligations outstanding"});
  return {cycle_, generation_, mode_, events_.snapshot(), completions_.snapshot(), std::move(reasons)};
}

DrainResult SystemLifecycle::drain(std::uint64_t budget, Tick tick,
                                   std::function<std::vector<IdleReason>()> external_idle) {
  const Cycle start = cycle_;
  if ((mode_ != SystemMode::Running && mode_ != SystemMode::Draining) || !tick)
    return {false, start, cycle_, snapshot()};
  mode_ = SystemMode::Draining;
  for (std::uint64_t used = 0; used <= budget; ++used) {
    auto external = external_idle ? external_idle() : std::vector<IdleReason>{};
    if (events_.empty() && completions_.empty() && external.empty()) {
      mode_ = SystemMode::Drained;
      return {true, start, cycle_, snapshot()};
    }
    if (used == budget) break;
    tick();
  }
  auto external = external_idle ? external_idle() : std::vector<IdleReason>{};
  return {false, start, cycle_, snapshot(external)};
}

}  // namespace openhbx
