#include <cassert>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "openhbf/integration/direct_runner.h"

namespace {
using namespace openhbf;
using namespace openhbf::integration;

struct TerminalEvent { RequestToken token{}; };

class ScriptedPipeline final : public ISystemPipeline {
 public:
  void bind(SystemServices services) override {
    events = &services.events;
    terminal = std::move(services.terminal);
    issued = std::move(services.issued);
  }
  bool can_accept(const HostRequest&) const noexcept override { return ready; }
  void accept(RequestToken token, HostRequest request, Cycle now,
              Generation generation) noexcept override {
    assert(token == request.token);
    ++accepted;
    outstanding = token;
    issued(token, now);
    if (complete_on_event) {
      events->schedule(Cycle(now.value() + 1), EventPhase::FinalDelivery,
                       generation, EventHandlerId(1),
                       EventPayload::make(TerminalEvent{token}));
    }
  }
  void tick(Cycle, Generation) override {}
  void on_event(const Event& event) override {
    phases.push_back(event.envelope().phase);
    const auto token = event.payload().get<TerminalEvent>().token;
    terminal({token, event.envelope().generation, HostStatus::Success, {}, {},
              event.envelope().cycle, {}});
    outstanding = {};
  }
  void reset(Generation, Generation, Cycle) override { outstanding = {}; }
  bool idle() const noexcept override { return !is_valid_token(outstanding); }
  PipelineSnapshot snapshot() const override {
    return {idle(), idle() ? 0U : 1U, ready ? "ready" : "blocked"};
  }

  EventQueue* events = nullptr;
  TerminalSink terminal;
  IssuedSink issued;
  bool ready = true;
  bool complete_on_event = true;
  std::size_t accepted = 0;
  RequestToken outstanding{};
  std::vector<EventPhase> phases;
};

HostRequest read_request() {
  HostRequest request;
  request.opcode = HostOpcode::Read;
  request.address = 0;
  request.size_bytes = 64;
  return request;
}

void retry_has_no_side_effects() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  auto* probe = pipeline.get();
  probe->ready = false;
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  std::size_t callbacks = 0;
  const auto outcome = system.try_submit(read_request(),
                                         [&](const Completion&) { ++callbacks; });
  assert(outcome.result == SubmitResult::Retry && !is_valid_token(outcome.token));
  assert(probe->accepted == 0 && callbacks == 0);
  assert(system.snapshot().completions.accepted == 0);
}

void accepted_issued_terminal_and_phase() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  auto* probe = pipeline.get();
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  Completion delivered;
  const auto outcome = system.try_submit(read_request(),
      [&](const Completion& completion) { delivered = completion; });
  assert(outcome.result == SubmitResult::Accepted && is_valid_token(outcome.token));
  assert(system.snapshot().completions.issued == 1);
  system.tick();
  assert(delivered.token == outcome.token);
  assert(delivered.accepted_cycle == Cycle(0));
  assert(delivered.issue_cycle == Cycle(0));
  assert(delivered.complete_cycle == Cycle(1));
  assert(probe->phases == std::vector<EventPhase>{EventPhase::FinalDelivery});
  assert(system.idle());
}

void drain_is_terminal() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  system.try_submit(read_request(), [](const Completion&) {});
  const auto drained = system.drain(2);
  assert(drained.drained && !drained.snapshot.accepting);
  const auto retry = system.try_submit(read_request(), [](const Completion&) {});
  assert(retry.result == SubmitResult::Retry);
}

void reset_aborts_and_stale_event_is_ignored() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  auto* probe = pipeline.get();
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  HostStatus status = HostStatus::Success;
  std::size_t callbacks = 0;
  system.try_submit(read_request(), [&](const Completion& completion) {
    ++callbacks;
    status = completion.status;
  });
  system.reset();
  assert(callbacks == 1 && status == HostStatus::Aborted);
  system.tick();
  assert(callbacks == 1 && probe->phases.empty());
  assert(system.idle());
}

void invalid_request_throws_before_admission() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  auto* probe = pipeline.get();
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  auto invalid = read_request();
  invalid.address = 1;
  bool threw = false;
  try {
    system.try_submit(invalid, [](const Completion&) {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw && probe->accepted == 0);
  assert(system.snapshot().completions.accepted == 0);
}

void callback_reentry_cannot_complete_twice() {
  auto pipeline = std::make_unique<ScriptedPipeline>();
  auto* probe = pipeline.get();
  OpenHbfSystem system(OpenHbfConfig::defaults(), std::move(pipeline));
  std::size_t callbacks = 0;
  system.try_submit(read_request(), [&](const Completion&) {
    ++callbacks;
    const auto second = system.try_submit(read_request(), [](const Completion&) {});
    assert(second.result == SubmitResult::Accepted);
  });
  system.tick();
  assert(callbacks == 1 && probe->accepted == 2);
  system.tick();
  assert(system.idle());
}
}  // namespace

int main() {
  retry_has_no_side_effects();
  accepted_issued_terminal_and_phase();
  drain_is_terminal();
  reset_aborts_and_stale_event_is_ignored();
  invalid_request_throws_before_admission();
  callback_reentry_cannot_complete_twice();
}
