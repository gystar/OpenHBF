#include "openhbf/media/command_engine.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace openhbf::media {

CommandEngine::CommandEngine(
    std::size_t max_in_flight, const MediaTopology& topology,
    MediaStateStore& state, IPageStore& pages, NandTimingModel& timing,
    EatTable& eat, ReliabilityModel& reliability, BadBlockTable& bbt,
    RetirementMap& retirement, DieEnvironment& environment, MediaStats& stats,
    CommandDependencies dependencies)
    : max_in_flight_(max_in_flight), topology_(topology), state_(state),
      pages_(pages), timing_(timing), eat_(eat), reliability_(reliability),
      bbt_(bbt), retirement_(retirement), environment_(environment),
      stats_(stats), dependencies_(std::move(dependencies)) {
  if (max_in_flight_ == 0 || !dependencies_.schedule ||
      !dependencies_.complete || dependencies_.event_handler.value() == 0) {
    throw std::invalid_argument("CommandEngine requires capacity, scheduler, completion sink and handler ID");
  }
}

MediaToken CommandEngine::next_token() {
  if (next_token_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("media token space exhausted");
  }
  return MediaToken(next_token_++);
}

IssueResult CommandEngine::try_issue(const MediaCommand& command, Cycle now) {
  const auto shape = validate(command, topology_.geometry());
  if (!shape) {
    stats_.on_issue(SubmitState::Rejected, {}, command, now,
                    MediaStatus::InvalidCommand);
    return {SubmitState::Rejected, {}, MediaStatus::InvalidCommand,
            shape.error().message};
  }
  if (commands_.size() >= max_in_flight_) {
    stats_.on_issue(SubmitState::Busy, {}, command, now, MediaStatus::Busy);
    return {SubmitState::Busy, {}, MediaStatus::Busy, "media command table is full"};
  }
  const auto bad = bbt_.is_bad(command.address);
  if (!bad) {
    return {SubmitState::Rejected, {}, MediaStatus::InvalidAddress,
            bad.error().message};
  }
  if (bad.value()) {
    stats_.on_issue(SubmitState::Rejected, {}, command, now,
                    MediaStatus::BlockBad);
    return {SubmitState::Rejected, {}, MediaStatus::BlockBad, "target block is bad"};
  }
  const auto retired = retirement_.is_retired(command.address);
  if (!retired) {
    return {SubmitState::Rejected, {}, MediaStatus::InvalidAddress,
            retired.error().message};
  }
  if (retired.value()) {
    stats_.on_issue(SubmitState::Rejected, {}, command, now,
                    MediaStatus::CapacityRetired);
    return {SubmitState::Rejected, {}, MediaStatus::CapacityRetired,
            "target capacity is retired"};
  }
  const DieKey die{command.address.core_die, command.address.die};
  const auto environment = environment_.can_accept(die, command.op);
  if (!environment.accepted) {
    stats_.on_issue(SubmitState::Rejected, {}, command, now,
                    environment.status);
    return {SubmitState::Rejected, {}, environment.status,
            "target die is not available"};
  }
  const MediaStatus legality = state_.check(command);
  if (legality != MediaStatus::Success) {
    const SubmitState state = legality == MediaStatus::Busy
                                  ? SubmitState::Busy
                                  : SubmitState::Rejected;
    stats_.on_issue(state, {}, command, now, legality);
    return {state, {}, legality, "media state rejected command"};
  }

  auto selection = timing_.stages_for(command.op, command.address,
                                      command.page_class,
                                      environment_.snapshot().temperature_celsius);
  if (!selection) {
    stats_.on_issue(SubmitState::Rejected, {}, command, now,
                    MediaStatus::InvalidCommand);
    return {SubmitState::Rejected, {}, MediaStatus::InvalidCommand,
            selection.error().message};
  }

  const MediaToken token = next_token();
  std::vector<ScheduledStage> stages;
  for (const auto& spec : selection.value().stages) {
    stages.push_back({spec});
  }
  if (stages.empty()) {
    return {SubmitState::Rejected, {}, MediaStatus::InvalidCommand,
            "timing model returned no stages"};
  }

  auto state_reservation = state_.reserve(token, command);
  if (!state_reservation) {
    return {SubmitState::Rejected, {}, MediaStatus::InternalError,
            state_reservation.error().message};
  }
  std::optional<PendingPayload> pending;
  if (command.op == MediaOp::Program) {
    const auto page = topology_.resolve(command.address);
    auto staged = pages_.stage_program(page.value().key, command.payload);
    if (!staged) {
      (void)state_.abort(state_reservation.value(), command.generation);
      return {SubmitState::Rejected, {}, MediaStatus::InternalError,
              staged.error().message};
    }
    pending = staged.value();
  }

  Context context{token, command, now, state_reservation.value(), pending,
                  std::move(stages), 0, {}, MediaStatus::Success, {}, 0, false};
  try {
    auto scheduled = schedule_stage(context, now);
    if (!scheduled) {
      if (pending) (void)pages_.abort_program(*pending);
      (void)state_.abort(state_reservation.value(), command.generation);
      return {SubmitState::Rejected, {}, MediaStatus::InvalidCommand,
              scheduled.error().message};
    }
  } catch (...) {
    (void)eat_.cancel(token, now);
    if (pending) (void)pages_.abort_program(*pending);
    (void)state_.abort(state_reservation.value(), command.generation);
    throw;
  }
  commands_.emplace(token, std::move(context));
  stats_.on_issue(SubmitState::Accepted, token, command, now,
                  MediaStatus::Success);
  return {SubmitState::Accepted, token, MediaStatus::Success, {}};
}

Result<void> CommandEngine::schedule_stage(Context& context, Cycle now) {
  const DieKey die{context.command.address.core_die,
                   context.command.address.die};
  const auto availability = environment_.can_accept(die, context.command.op);
  if (!availability.accepted) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "target die became unavailable between stages"});
  }
  const auto& stage = context.stages.at(context.next_stage).spec;
  auto modified = environment_.stage_modifier(stage.duration);
  if (!modified) return Result<void>::failure(modified.error());
  auto reservation = eat_.reserve_atomic(
      context.token, stage.resources, now, modified.value().duration);
  if (!reservation) return Result<void>::failure(reservation.error());
  context.active_reservation = reservation.value();
  dependencies_.schedule(reservation.value().end, EventPhase::MediaCommit,
      context.command.generation, dependencies_.event_handler,
      EventPayload::make(StageEvent{context.token, context.command.generation,
                                    context.next_stage}));
  stats_.on_stage(context.token, context.command.generation,
      context.command.address, context.command.op, stage.kind,
      reservation.value().start, reservation.value().end, stage.resources);
  return Result<void>::success();
}

MediaCompletion CommandEngine::finish(
    Context& context, MediaStatus status, Cycle now,
    std::optional<PayloadSnapshot> payload, std::uint32_t raw_errors,
    bool refresh_recommended) {
  return {context.token, context.command.origin, context.command.generation,
          context.command.op, context.command.address, status,
          context.accepted, now, std::move(payload), raw_errors,
          refresh_recommended};
}

void CommandEngine::on_event(const StageEvent& event, Cycle now) {
  auto found = commands_.find(event.token);
  if (found == commands_.end()) {
    // Reset cannot remove an already queued system event. A generation that
    // was explicitly cancelled is therefore a known stale event, not a
    // duplicate terminal completion.
    if (cancelled_generations_.find(event.generation) !=
        cancelled_generations_.end()) {
      return;
    }
    throw std::logic_error("unknown or duplicate media event token");
  }
  Context& context = found->second;
  if (event.generation != context.command.generation ||
      event.stage_index != context.next_stage ||
      !context.active_reservation ||
      now < context.active_reservation->end) {
    throw std::logic_error("stale or malformed media stage event");
  }
  (void)eat_.cancel(context.token, now);

  const auto resolved = topology_.resolve(context.command.address).value();
  const BlockState block = state_.block_state(resolved.block_key);
  const StageKind current_stage = context.stages[event.stage_index].spec.kind;
  const DieKey die{context.command.address.core_die,
                   context.command.address.die};
  const auto availability = environment_.can_accept(die, context.command.op);
  if (!availability.accepted) {
    context.status = availability.status;
  }
  const std::uint64_t program_age = now < block.last_program_cycle
      ? 0 : now.value() - block.last_program_cycle.value();
  ReliabilityInput reliability_input{context.token, context.command.address,
                                     context.command.op, current_stage,
                                     block.epoch, block.program_erase_count,
                                     program_age, block.read_count,
                                     environment_.snapshot().temperature_celsius};

  if (context.status == MediaStatus::Success &&
      current_stage == StageKind::ReadSense) {
    const auto raw = reliability_.evaluate_read(reliability_input);
    context.raw_errors = raw.raw_errors;
    if (raw.uncorrectable) context.status = MediaStatus::Uncorrectable;
    else if (raw.retry_recommended) context.status = MediaStatus::RetryRequired;
    if (context.status == MediaStatus::Success) {
      auto snapshot = pages_.snapshot(resolved.key, context.command.address,
                                      block.epoch);
      if (!snapshot) context.status = MediaStatus::InternalError;
      else context.payload = std::move(snapshot.value());
    }
    if (context.status == MediaStatus::Success) {
      const auto read_count = state_.increment_read_count(resolved.block_key);
      context.refresh_recommended = reliability_.on_successful_sense(
          context.command.address, block.epoch, read_count).has_value();
    }
  } else if (context.status == MediaStatus::Success &&
             current_stage == StageKind::ProgramArray) {
    if (reliability_.evaluate_program(reliability_input).failed) {
      context.status = MediaStatus::ProgramFailure;
      (void)pages_.abort_program(*context.pending_payload);
      (void)bbt_.mark_bad(context.command.address, BadReason::ProgramFailure,
                          now, block.program_erase_count);
    }
  } else if (context.status == MediaStatus::Success &&
             current_stage == StageKind::EraseArray) {
    if (reliability_.evaluate_erase(reliability_input).failed) {
      context.status = MediaStatus::EraseFailure;
      (void)bbt_.mark_bad(context.command.address, BadReason::EraseFailure,
                          now, block.program_erase_count);
    }
  }

  ++context.next_stage;
  context.active_reservation.reset();
  if (context.status == MediaStatus::Success &&
      context.next_stage != context.stages.size()) {
    auto scheduled = schedule_stage(context, now);
    if (scheduled) return;
    context.status = MediaStatus::InternalError;
    context.next_stage = static_cast<std::uint32_t>(context.stages.size());
  }
  if (context.status != MediaStatus::Success) {
    context.next_stage = static_cast<std::uint32_t>(context.stages.size());
  }

  MediaStatus status = context.status;
  if (status == MediaStatus::Success && context.command.op == MediaOp::Program) {
    auto committed = pages_.commit_program(*context.pending_payload);
    if (!committed) status = MediaStatus::InternalError;
  } else if (status != MediaStatus::Success && context.pending_payload) {
    (void)pages_.abort_program(*context.pending_payload);
  }

  StateDelta delta{status == MediaStatus::Success,
                   status == MediaStatus::ProgramFailure};
  auto state_commit = state_.commit_terminal(
      context.state_reservation, context.command.generation, delta, now);
  if (!state_commit) status = MediaStatus::InternalError;
  if (status == MediaStatus::Success && context.command.op == MediaOp::Erase) {
    const auto block_ref = topology_.resolve_block(context.command.address).value();
    pages_.erase_block(block_ref);
  }

  MediaCompletion completion = finish(context, status, now,
      std::move(context.payload), context.raw_errors,
      context.refresh_recommended);
  stats_.on_complete(completion);
  commands_.erase(found);
  dependencies_.complete(std::move(completion));
}

void CommandEngine::cancel_generation(Generation generation, Cycle now) {
  cancelled_generations_.insert(generation);
  std::vector<MediaToken> cancelled;
  for (const auto& entry : commands_) {
    if (entry.second.command.generation == generation) cancelled.push_back(entry.first);
  }
  for (MediaToken token : cancelled) {
    auto found = commands_.find(token);
    Context& context = found->second;
    if (context.pending_payload) (void)pages_.abort_program(*context.pending_payload);
    (void)eat_.cancel(token, now);
    (void)state_.abort(context.state_reservation, generation);
    MediaCompletion completion = finish(context, MediaStatus::Aborted, now);
    stats_.on_complete(completion);
    commands_.erase(found);
    dependencies_.complete(std::move(completion));
  }
}

}  // namespace openhbf::media
