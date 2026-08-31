#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "openhbx/common/admission.h"
#include "openhbx/common/checked_math.h"
#include "openhbx/common/payload_handle.h"
#include "openhbx/common/strong_types.h"
#include "openhbx/system/clock_domain.h"
#include "openhbx/system/completion_registry.h"
#include "openhbx/system/event_queue.h"
#include "openhbx/system/observability.h"
#include "openhbx/system/system_lifecycle.h"

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " CHECK(" #x ") failed\n"; ++failures; } } while (false)

void test_common_contracts() {
  static_assert(!std::is_convertible<std::uint64_t, openhbx::Cycle>::value, "strong cycle");
  CHECK(openhbx::checked_add(4, 5).value() == 9);
  CHECK(!openhbx::checked_add(std::numeric_limits<std::uint64_t>::max(), 1));
  CHECK(!openhbx::checked_mul(std::numeric_limits<std::uint64_t>::max(), 2));
  auto payload = openhbx::PayloadHandle::from_bytes({1, 2, 3});
  auto snapshot = payload;
  CHECK(snapshot.shares_storage_with(payload));
  CHECK(snapshot.bytes()[1] == 2);
}

void test_event_order_and_rejection() {
  using namespace openhbx;
  EventQueue queue;
  std::vector<std::uint64_t> seen;
  CHECK(queue.register_handler(HandlerId(1), [&](EventPayload value) { seen.push_back(std::get<std::uint64_t>(value)); }));
  EventSpec a{Cycle(4), EventPhase::FinalDelivery, HandlerId(1), Generation(1), std::uint64_t(3)};
  EventSpec b{Cycle(4), EventPhase::MediaCommit, HandlerId(1), Generation(1), std::uint64_t(1)};
  EventSpec c{Cycle(4), EventPhase::MediaCommit, HandlerId(1), Generation(1), std::uint64_t(2)};
  CHECK(queue.schedule(a, Cycle(3), EventPhase::Reset) == ScheduleCode::Accepted);
  CHECK(queue.schedule(b, Cycle(3), EventPhase::Reset) == ScheduleCode::Accepted);
  CHECK(queue.schedule(c, Cycle(3), EventPhase::Reset) == ScheduleCode::Accepted);
  EventSpec rejected{Cycle(3), EventPhase::Reset, HandlerId(1), Generation(1), PayloadHandle::from_bytes({9})};
  CHECK(queue.schedule(rejected, Cycle(3), EventPhase::Reset) == ScheduleCode::ClosedPhase);
  CHECK(!std::get<PayloadHandle>(rejected.payload).empty());
  queue.dispatch_due(Cycle(4), EventPhase::MediaCommit, Generation(1));
  queue.dispatch_due(Cycle(4), EventPhase::FinalDelivery, Generation(1));
  CHECK((seen == std::vector<std::uint64_t>{1, 2, 3}));
  CHECK(queue.snapshot().scheduled == 3 && queue.snapshot().dispatched == 3);
}

void test_structured_observability() {
  using namespace openhbx;
  EventJournal journal(2, LogLevel::Trace);
  std::vector<std::string> rendered;
  journal.set_sink([&](const ObservedEvent& event) {
    rendered.push_back(render_event_text(event));
  });
  EventQueue queue(&journal);
  CHECK(queue.register_handler(HandlerId(8), [](EventPayload) {}));
  EventSpec accepted{Cycle(2), EventPhase::MediaCommit, HandlerId(8),
                     Generation(3), Token(17)};
  EventSpec rejected{Cycle(0), EventPhase::Reset, HandlerId(8),
                     Generation(3), Token(18)};
  CHECK(queue.schedule(accepted, Cycle(0), EventPhase::Reset) ==
        ScheduleCode::Accepted);
  CHECK(queue.schedule(rejected, Cycle(0), EventPhase::Reset) ==
        ScheduleCode::ClosedPhase);
  CHECK(queue.dispatch_due(Cycle(2), EventPhase::MediaCommit,
                           Generation(3)) == 1);

  const auto snapshot = journal.snapshot();
  CHECK(snapshot.observed == 3);
  CHECK(snapshot.events.size() == 2);
  CHECK(snapshot.dropped == 1);
  CHECK(snapshot.events[0].sequence == 0);
  CHECK(snapshot.events[0].token == Token(17));
  CHECK(snapshot.events[1].result == "closed_phase");
  CHECK(rendered.size() == 2);
  CHECK(rendered[0].find("cycle=2 phase=media_commit sequence=0") !=
        std::string::npos);
  const auto json = render_event_jsonl(snapshot.events[1]);
  CHECK(json.find("\"module\":\"event_queue\"") != std::string::npos);
  CHECK(json.find("\"result\":\"closed_phase\"") != std::string::npos);

  EventJournal filtered(4, LogLevel::Warn);
  filtered.observe({LogLevel::Trace, Cycle(1), EventPhase::Reset, 0, false,
                    Generation(1), "test", "trace", "stage", Token{}, "ok", 0});
  filtered.observe({LogLevel::Error, Cycle(1), EventPhase::Reset, 0, false,
                    Generation(1), "test", "error", "stage", Token{}, "failed", 0});
  const auto filtered_snapshot = filtered.snapshot();
  CHECK(filtered_snapshot.observed == 2);
  CHECK(filtered_snapshot.filtered == 1);
  CHECK(filtered_snapshot.events.size() == 1);
}

void test_completion_and_reset() {
  using namespace openhbx;
  CompletionRegistry registry(1);
  int callbacks = 0;
  CHECK(registry.register_request(Token(1), Generation(1), [&](Completion result) {
    ++callbacks; CHECK(result.code == TerminalCode::Success);
  }).code == AdmissionCode::Accepted);
  CHECK(registry.register_request(Token(2), Generation(1), [](Completion) {}).code == AdmissionCode::Busy);
  CHECK(registry.mark_issued(Token(1), Generation(1)));
  CHECK(registry.finish_once({Token(1), Generation(1), TerminalCode::Success, false, {}}) == FinishCode::Delivered);
  CHECK(registry.finish_once({Token(1), Generation(1), TerminalCode::Success, false, {}}) == FinishCode::DuplicateToken);
  CHECK(callbacks == 1);
  auto state = registry.snapshot();
  CHECK(state.accepted == state.terminal + state.outstanding);

  EventQueue queue;
  int stale_handler_calls = 0;
  queue.register_handler(HandlerId(1), [&](EventPayload) { ++stale_handler_calls; });
  EventSpec old{Cycle(2), EventPhase::MediaCommit, HandlerId(1), Generation(1), {}};
  CHECK(queue.schedule(old, Cycle(0), EventPhase::Reset) == ScheduleCode::Accepted);
  CompletionRegistry resetting(2);
  int aborted = 0;
  resetting.register_request(Token(4), Generation(1), [&](Completion result) {
    if (result.code == TerminalCode::AbortedByReset) ++aborted;
  });
  SystemLifecycle lifecycle(queue, resetting);
  CHECK(lifecycle.reset());
  CHECK(lifecycle.generation() == Generation(2));
  CHECK(aborted == 1 && queue.empty());
  CHECK(stale_handler_calls == 0 && queue.snapshot().stale == 1);
}

void test_final_delivery_generation_rebase() {
  using namespace openhbx;
  EventQueue queue;
  std::vector<Token> delivered;
  CHECK(queue.register_handler(HandlerId(7), [&](EventPayload payload) {
    const auto* token = std::get_if<Token>(&payload);
    CHECK(token != nullptr);
    if (token) delivered.push_back(*token);
  }));
  EventSpec final{Cycle(9), EventPhase::FinalDelivery, HandlerId(7),
                  Generation(3), Token(41)};
  CHECK(queue.schedule(final, Cycle(4), EventPhase::Reset) ==
        ScheduleCode::Accepted);
  const auto before = queue.snapshot();
  CHECK(before.queued == 1 && before.events.size() == 1);
  CHECK(queue.rebase_generation(Generation(3), Generation(4), HandlerId(7),
                                EventPhase::FinalDelivery) == 1);
  const auto after = queue.snapshot();
  CHECK(after.queued == before.queued);
  CHECK(after.scheduled == before.scheduled);
  CHECK(after.dispatched == before.dispatched);
  CHECK(after.stale == before.stale);
  CHECK(after.events.size() == 1);
  if (before.events.size() == 1 && after.events.size() == 1) {
    CHECK(after.events[0].due == before.events[0].due);
    CHECK(after.events[0].phase == before.events[0].phase);
    CHECK(after.events[0].sequence == before.events[0].sequence);
    CHECK(after.events[0].handler == before.events[0].handler);
    CHECK(before.events[0].generation == Generation(3));
    CHECK(after.events[0].generation == Generation(4));
  }
  CHECK(queue.dispatch_due(Cycle(9), EventPhase::FinalDelivery,
                           Generation(4)) == 1);
  CHECK(delivered.size() == 1 && delivered.front() == Token(41));
  CHECK(queue.snapshot().dispatched == before.dispatched + 1);
}

void test_clock_and_drain_snapshot() {
  using namespace openhbx;
  ClockDomain slow(1, 3);
  CHECK(slow.ticks_due() == 0); CHECK(slow.ticks_due() == 0); CHECK(slow.ticks_due() == 1);
  ClockDomain fast(3, 2);
  CHECK(fast.ticks_due() == 1); CHECK(fast.ticks_due() == 2);
  EventQueue queue;
  CompletionRegistry registry(2);
  SystemLifecycle lifecycle(queue, registry);
  const auto before = lifecycle.snapshot();
  const auto again = lifecycle.snapshot();
  CHECK(before.cycle == again.cycle && before.events.queued == again.events.queued);
  const auto drained = lifecycle.drain(0, [&] { lifecycle.advance_cycle(); });
  CHECK(drained.drained && drained.start == drained.end);

  EventQueue blocked_queue;
  CompletionRegistry blocked_registry(2);
  blocked_registry.register_request(Token(9), Generation(1), [](Completion) {});
  SystemLifecycle blocked(blocked_queue, blocked_registry);
  auto timeout = blocked.drain(2, [&] { blocked.advance_cycle(); });
  CHECK(!timeout.drained && timeout.end == Cycle(2));
  CHECK(!timeout.snapshot.non_idle.empty());
}
}  // namespace

int main() {
  test_common_contracts();
  test_event_order_and_rejection();
  test_structured_observability();
  test_completion_and_reset();
  test_final_delivery_generation_rebase();
  test_clock_and_drain_snapshot();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
