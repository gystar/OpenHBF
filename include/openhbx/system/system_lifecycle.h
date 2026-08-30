#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "openhbx/common/strong_types.h"
#include "openhbx/system/completion_registry.h"
#include "openhbx/system/event_queue.h"

namespace openhbx {

enum class SystemMode { Running, Resetting, Draining, Drained };
struct IdleReason { std::string owner; std::string reason; };
struct SystemSnapshot { Cycle cycle; Generation generation; SystemMode mode; EventQueueSnapshot events; CompletionSnapshot completions; std::vector<IdleReason> non_idle; };
struct DrainResult { bool drained; Cycle start; Cycle end; SystemSnapshot snapshot; };

class SystemLifecycle {
 public:
  using Tick = std::function<void()>;
  SystemLifecycle(EventQueue& events, CompletionRegistry& completions);
  Cycle cycle() const noexcept { return cycle_; }
  Generation generation() const noexcept { return generation_; }
  SystemMode mode() const noexcept { return mode_; }
  void advance_cycle();
  bool reset();
  bool reset_without_completion_abort();
  SystemSnapshot snapshot(const std::vector<IdleReason>& external = {}) const;
  DrainResult drain(std::uint64_t budget, Tick tick,
                    std::function<std::vector<IdleReason>()> external_idle = {});
 private:
  EventQueue& events_;
  CompletionRegistry& completions_;
  Cycle cycle_{0};
  Generation generation_{1};
  SystemMode mode_{SystemMode::Running};
};

}  // namespace openhbx
