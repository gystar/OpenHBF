#include "openhbf/integration/open_hbf_system.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace openhbf::integration {

OpenHbfSystem::OpenHbfSystem(OpenHbfConfig config,
                             std::unique_ptr<ISystemPipeline> pipeline,
                             CallbackErrorSink callback_errors)
    : config_(std::move(config)), completions_(std::move(callback_errors)),
      pipeline_(std::move(pipeline)) {
  const auto valid = config_.validate();
  if (!valid) throw std::invalid_argument(valid.error().message);
  if (!pipeline_) throw std::invalid_argument("system pipeline must not be null");
  pipeline_->bind(SystemServices{
      events_, [this](Completion completion) { finish(std::move(completion)); },
      [this](RequestToken token, Cycle cycle) { mark_issued(token, cycle); }});
}

OpenHbfSystem::~OpenHbfSystem() {
  completions_.abort_generation(generation_, HostStatus::Aborted, cycle_);
}

SubmitOutcome OpenHbfSystem::try_submit(HostRequest request,
                                        CompletionSink completion) {
  const auto valid = validate(request, config_.host.channels,
                              config_.host.axi_interfaces_per_channel,
                              config_.payload_mode);
  if (!valid) throw std::invalid_argument(valid.error().message);
  if (!accepting_ || !pipeline_->can_accept(request))
    return {SubmitResult::Retry, RequestToken{}};
  if (!completion) throw std::invalid_argument("completion sink must not be empty");
  const auto token = completions_.register_request(
      std::move(completion), generation_, cycle_);
  request.token = token;
  pipeline_->accept(token, std::move(request), cycle_, generation_);
  return {SubmitResult::Accepted, token};
}

void OpenHbfSystem::mark_issued(RequestToken token, Cycle cycle) {
  const auto result = completions_.mark_issued(token, cycle);
  if (!result) throw std::logic_error(result.error().message);
}

void OpenHbfSystem::finish(Completion completion) {
  if (completion.generation != generation_) return;
  const auto result = completions_.finish_once(std::move(completion));
  if (!result) throw std::logic_error(result.error().message);
}

void OpenHbfSystem::dispatch_ready_events() {
  for (std::uint8_t value = event_phase_order(EventPhase::Reset);
       value <= event_phase_order(EventPhase::FinalDelivery); ++value) {
    const auto phase = static_cast<EventPhase>(value);
    while (auto event = events_.pop_ready(phase)) {
      if (event->envelope().generation == generation_) pipeline_->on_event(*event);
    }
  }
}

void OpenHbfSystem::tick() {
  if (cycle_.value() == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("system cycle space exhausted");
  // Cycle zero has not passed through a phase frontier yet. A constructor or
  // initial admission may legitimately enqueue work there; later admissions
  // must target a future cycle because the prior tick closed all phases.
  if (cycle_.value() == 0) dispatch_ready_events();
  cycle_ = Cycle(cycle_.value() + 1);
  events_.advance_to(cycle_);
  pipeline_->tick(cycle_, generation_);
  dispatch_ready_events();
}

bool OpenHbfSystem::idle() const noexcept {
  return pipeline_->idle() && events_.empty() && completions_.empty();
}

SystemSnapshot OpenHbfSystem::snapshot() const {
  return {cycle_, generation_, events_.size(), completions_.snapshot(),
          pipeline_->snapshot(), accepting_};
}

DrainResult OpenHbfSystem::drain(std::uint64_t max_cycles) {
  accepting_ = false;
  std::uint64_t elapsed = 0;
  while (!idle() && elapsed < max_cycles) {
    tick();
    ++elapsed;
  }
  return {idle(), elapsed, snapshot()};
}

void OpenHbfSystem::reset() {
  if (generation_.value() == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("system generation space exhausted");
  const auto old_generation = generation_;
  generation_ = Generation(generation_.value() + 1);
  accepting_ = false;
  pipeline_->reset(old_generation, generation_, cycle_);
  completions_.abort_generation(old_generation, HostStatus::Aborted, cycle_);
  events_.discard_generation(old_generation);
  accepting_ = true;
}

}  // namespace openhbf::integration
