#include <cassert>
#include <stdexcept>

#include "openhbf/integration/completion_registry.h"
#include "openhbf/integration/open_hbf_config.h"

using namespace openhbf;
using namespace openhbf::integration;

int main() {
  auto config = OpenHbfConfig::defaults();
  assert(config.validate());
  assert(config.canonical_dump() == config.canonical_dump());

  HostRequest request;
  request.opcode = HostOpcode::Write;
  request.payload = std::make_shared<const PayloadBytes>(64, 0x5a);
  assert(validate(request, 1));
  request.payload.reset();
  assert(validate(request, 1, 1, PayloadMode::TimingOnly));
  assert(!validate(request, 1, 1, PayloadMode::Functional));
  request.payload = std::make_shared<const PayloadBytes>(64, 0x5a);
  request.address = 4096 - 64;
  request.size_bytes = 128;
  assert(!validate(request, 1));

  std::size_t called = 0;
  CompletionRegistry registry;
  const auto token = registry.register_request(
      [&](const Completion& c) { ++called; assert(c.issue_cycle == Cycle(2)); },
      Generation(1), Cycle(1));
  assert(registry.mark_issued(token, Cycle(2)));
  assert(registry.finish_once(
      Completion{token, Generation(1), HostStatus::Success, {}, {}, Cycle(3), {}}));
  assert(called == 1 && registry.empty());
  assert(!registry.finish_once(
      Completion{token, Generation(1), HostStatus::Success, {}, {}, Cycle(4), {}}));

  const auto ordered = registry.register_request(
      [&](const Completion&) { ++called; }, Generation(1), Cycle(10));
  assert(!registry.finish_once(
      Completion{ordered, Generation(1), HostStatus::Success, {}, {}, Cycle(9), {}}));
  assert(registry.finish_once(
      Completion{ordered, Generation(1), HostStatus::Success, {}, {}, Cycle(10), {}}));
  assert(called == 2);

  const auto throwing = registry.register_request(
      [](const Completion&) { throw std::runtime_error("callback"); },
      Generation(2), Cycle(4));
  auto result = registry.finish_once(
      Completion{throwing, Generation(2), HostStatus::Success, {}, {}, Cycle(5), {}});
  assert(result && result.value() == FinishResult::CallbackFailed);
  assert(registry.snapshot().callback_failures == 1);

  registry.register_request([](const Completion&) {}, Generation(3), Cycle(5));
  registry.register_request([](const Completion&) {}, Generation(4), Cycle(5));
  assert(registry.abort_generation(Generation(3), HostStatus::Aborted,
                                   Cycle(6)) == 1);
  assert(registry.snapshot().outstanding == 1);
}
