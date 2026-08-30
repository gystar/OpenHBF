#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "openhbf/media/bbt.h"
#include "openhbf/media/die_environment.h"
#include "openhbf/media/eat.h"
#include "openhbf/media/media_stats.h"
#include "openhbf/media/page_store.h"
#include "openhbf/media/reliability.h"
#include "openhbf/media/state.h"
#include "openhbf/media/timing.h"

namespace openhbf::media {

using EventScheduler =
    std::function<EventToken(Cycle, EventPhase, Generation, EventHandlerId,
                             EventPayload)>;

struct CommandDependencies {
  EventScheduler schedule;
  CompletionSink complete;
  EventHandlerId event_handler;
};

// CommandEngine owns only in-flight command contexts. Persistent media state,
// payload, resource availability and reliability counters remain in their
// dedicated owners and are changed only by the terminal event path.
class CommandEngine {
 public:
  CommandEngine(std::size_t max_in_flight, const MediaTopology& topology,
                MediaStateStore& state, IPageStore& pages,
                NandTimingModel& timing, EatTable& eat,
                ReliabilityModel& reliability, BadBlockTable& bbt,
                RetirementMap& retirement, DieEnvironment& environment,
                MediaStats& stats, CommandDependencies dependencies);

  IssueResult try_issue(const MediaCommand& command, Cycle now);
  void on_event(const StageEvent& event, Cycle now);
  void cancel_generation(Generation generation, Cycle now);
  bool idle() const noexcept { return commands_.empty(); }
  std::size_t in_flight() const noexcept { return commands_.size(); }

 private:
  struct ScheduledStage {
    StageSpec spec;
  };
  struct Context {
    MediaToken token;
    MediaCommand command;
    Cycle accepted;
    ReservationHandle state_reservation;
    std::optional<PendingPayload> pending_payload;
    std::vector<ScheduledStage> stages;
    std::uint32_t next_stage = 0;
    std::optional<Reservation> active_reservation;
    MediaStatus status = MediaStatus::Success;
    std::optional<PayloadSnapshot> payload;
    std::uint32_t raw_errors = 0;
    bool refresh_recommended = false;
  };

  MediaToken next_token();
  Result<void> schedule_stage(Context& context, Cycle now);
  MediaCompletion finish(Context& context, MediaStatus status, Cycle now,
                         std::optional<PayloadSnapshot> payload = {},
                         std::uint32_t raw_errors = 0,
                         bool refresh_recommended = false);

  std::size_t max_in_flight_;
  std::uint64_t next_token_ = 1;
  const MediaTopology& topology_;
  MediaStateStore& state_;
  IPageStore& pages_;
  NandTimingModel& timing_;
  EatTable& eat_;
  ReliabilityModel& reliability_;
  BadBlockTable& bbt_;
  RetirementMap& retirement_;
  DieEnvironment& environment_;
  MediaStats& stats_;
  CommandDependencies dependencies_;
  std::unordered_map<MediaToken, Context> commands_;
  std::unordered_set<Generation> cancelled_generations_;
};

}  // namespace openhbf::media
