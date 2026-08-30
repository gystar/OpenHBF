#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "openhbf/common/event_queue.h"
#include "openhbf/integration/completion_registry.h"
#include "openhbf/integration/open_hbf_config.h"

namespace openhbf::integration {

using TerminalSink = std::function<void(Completion)>;
using IssuedSink = std::function<void(RequestToken, Cycle)>;

struct SystemServices {
  EventQueue& events;
  TerminalSink terminal;
  IssuedSink issued;
};

struct PipelineSnapshot {
  bool idle = true;
  std::size_t outstanding = 0;
  std::string detail;
};

// Temporary narrow assembly boundary while the production 01/03/04/05 object
// graph is being completed. Implementations use the supplied EventQueue and
// callbacks; they must not create another clock or completion path.
class ISystemPipeline {
 public:
  virtual ~ISystemPipeline() = default;
  virtual void bind(SystemServices services) = 0;
  virtual bool can_accept(const HostRequest& request) const noexcept = 0;
  virtual void accept(RequestToken token, HostRequest request, Cycle now,
                      Generation generation) noexcept = 0;
  virtual void tick(Cycle now, Generation generation) = 0;
  virtual void on_event(const Event& event) = 0;
  virtual void reset(Generation old_generation, Generation new_generation,
                     Cycle now) = 0;
  virtual bool idle() const noexcept = 0;
  virtual PipelineSnapshot snapshot() const = 0;
};

using IRequestEndpoint = ISystemPipeline;

struct SystemSnapshot {
  Cycle cycle{};
  Generation generation{};
  std::size_t queued_events = 0;
  CompletionSnapshot completions;
  PipelineSnapshot pipeline;
  bool accepting = true;
};

struct DrainResult {
  bool drained = false;
  std::uint64_t cycles = 0;
  SystemSnapshot snapshot;
};

class OpenHbfSystem final {
 public:
  explicit OpenHbfSystem(OpenHbfConfig config,
                         std::unique_ptr<ISystemPipeline> pipeline,
                         CallbackErrorSink callback_errors = {});
  ~OpenHbfSystem();
  OpenHbfSystem(const OpenHbfSystem&) = delete;
  OpenHbfSystem& operator=(const OpenHbfSystem&) = delete;

  SubmitOutcome try_submit(HostRequest request, CompletionSink completion);
  void tick();
  DrainResult drain(std::uint64_t max_cycles);
  void reset();

  bool idle() const noexcept;
  SystemSnapshot snapshot() const;
  Cycle current_cycle() const noexcept { return cycle_; }
  Generation generation() const noexcept { return generation_; }
  EventQueue& event_queue() noexcept { return events_; }
  const EventQueue& event_queue() const noexcept { return events_; }
  const OpenHbfConfig& config() const noexcept { return config_; }
  CapabilityManifest capabilities() const noexcept { return config_.manifest(); }

 private:
  void dispatch_ready_events();
  void finish(Completion completion);
  void mark_issued(RequestToken token, Cycle cycle);

  Cycle cycle_{};
  Generation generation_{};
  bool accepting_ = true;
  OpenHbfConfig config_;
  EventQueue events_;
  CompletionRegistry completions_;
  std::unique_ptr<ISystemPipeline> pipeline_;
};

}  // namespace openhbf::integration
