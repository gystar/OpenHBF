#include "openhbx/media/nand/nand_flash_device.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "openhbx/common/checked_math.h"

namespace openhbx::media::nand {
namespace {
Cycle later(Cycle a, Cycle b) { return a < b ? b : a; }
MediaStatus raw_status(ras::RawReadClass value) {
  switch (value) {
    case ras::RawReadClass::Clean: return MediaStatus::Success;
    case ras::RawReadClass::Correctable: return MediaStatus::RawCorrectable;
    case ras::RawReadClass::Uncorrectable: return MediaStatus::RawUncorrectable;
    case ras::RawReadClass::Retry: return MediaStatus::RetrySuggested;
    case ras::RawReadClass::Refresh: return MediaStatus::RefreshNotice;
  }
  return MediaStatus::IntegrityError;
}
}  // namespace

NandFlashDevice::NandFlashDevice(NandDeviceConfig config, EventQueue& events,
                                 HandlerId handler, CompletionSink completion)
    : config_(std::move(config)), topology_(config_.geometry), events_(events),
      handler_(handler), completion_(std::move(completion)), reliability_(config_.reliability) {
  if (!events_.register_handler(handler_, [this](EventPayload payload) { on_event(std::move(payload)); }))
    throw std::invalid_argument("duplicate media event handler");
}

MediaStatus NandFlashDevice::validate(const FlashCommand& cmd) const {
  const auto page = topology_.flatten_page(cmd.address);
  const auto block = topology_.flatten_block(cmd.address);
  if (!page || !block) return MediaStatus::InvalidAddress;
  const std::uint64_t total_blocks = config_.geometry.core_dies *
      config_.geometry.dies_per_core * config_.geometry.banks_per_die *
      config_.geometry.blocks_per_bank;
  if (bbt_.size() >= total_blocks) return MediaStatus::CapacityUnusable;
  if (bbt_.is_bad(*block)) return MediaStatus::BadBlock;
  if (cmd.kind == CommandKind::Program) {
    if (cmd.payload.size() != config_.geometry.page_bytes) return MediaStatus::InvalidPayload;
    if (pages_.count(*page)) return MediaStatus::InvalidState;
  }
  return MediaStatus::Success;
}

AdmissionResult NandFlashDevice::try_issue(FlashCommand command, Cycle now) {
  const MediaStatus status = validate(command);
  if (status != MediaStatus::Success) {
    return AdmissionResult::rejected(RejectionReason::InvalidArgument, "media command rejected");
  }
  if (contexts_.size() >= config_.max_inflight || contexts_.count(command.token.value()))
    return AdmissionResult::busy();
  if (command.generation != generation_)
    return AdmissionResult::rejected(RejectionReason::InvalidLifecycle, "stale generation");
  Stage first = Stage::ReadCommand;
  if (command.kind == CommandKind::Program) first = Stage::ProgramData;
  if (command.kind == CommandKind::Erase) first = Stage::EraseSetup;
  const auto bank = topology_.bank_id(command.address);
  const auto die = topology_.die_id(command.address);
  Cycle start = later(now, bank_eat_[bank]);
  if (uses_die(first)) start = later(start, die_eat_[die]);
  if (!checked_add(start.value(), duration(first)))
    return AdmissionResult::rejected(RejectionReason::CapacityImpossible,
                                     "media stage cycle overflow");
  Context context{std::move(command), first, now, 0, {}};
  const auto token = context.command.token.value();
  contexts_.emplace(token, std::move(context));
  if (!schedule(contexts_.at(token), now, first)) {
    contexts_.erase(token);
    return AdmissionResult::rejected(RejectionReason::CapacityImpossible,
                                     "media stage cycle overflow");
  }
  return AdmissionResult::accepted();
}

std::uint64_t NandFlashDevice::duration(Stage stage) const {
  switch (stage) {
    case Stage::ReadCommand: return config_.timing.read_command;
    case Stage::ReadSense: return config_.timing.read_sense;
    case Stage::ProgramData: return config_.timing.program_data_in;
    case Stage::ProgramArray: return config_.timing.program_array;
    case Stage::ProgramVerify: return config_.timing.program_verify;
    case Stage::EraseSetup: return config_.timing.erase_setup;
    case Stage::EraseArray: return config_.timing.erase_array;
    case Stage::EraseVerify: return config_.timing.erase_verify;
  }
  return 0;
}
bool NandFlashDevice::uses_die(Stage stage) const {
  return stage == Stage::ReadCommand || stage == Stage::ProgramData || stage == Stage::EraseSetup;
}

bool NandFlashDevice::schedule(Context& context, Cycle now, Stage next) {
  const auto bank = topology_.bank_id(context.command.address);
  const auto die = topology_.die_id(context.command.address);
  Cycle start = later(now, bank_eat_[bank]);
  if (uses_die(next)) start = later(start, die_eat_[die]);
  const auto end_value = checked_add(start.value(), duration(next));
  if (!end_value) return false;
  const Cycle end(*end_value);
  bank_eat_[bank] = end;
  if (uses_die(next)) die_eat_[die] = end;
  context.stage = next;
  context.due = end;
  ++context.sequence;
  EventSpec event{end, EventPhase::MediaCommit, handler_, context.command.generation,
                  EventPayload(context.command.token)};
  const auto code = events_.schedule(event, now, EventPhase::Reset);
  if (code != ScheduleCode::Accepted) throw std::logic_error("media event schedule failed");
  return true;
}

void NandFlashDevice::on_event(EventPayload payload) {
  const auto token = std::get_if<Token>(&payload);
  if (!token) { ++stale_events_; return; }
  auto it = contexts_.find(token->value());
  if (it == contexts_.end()) { ++stale_events_; return; }
  Context& ctx = it->second;
  const Cycle now = ctx.due;
  if (blocked_dies_.count(topology_.die_id(ctx.command.address))) {
    terminal(ctx, MediaStatus::DieTemporarilyBlocked, now, false);
    return;
  }
  switch (ctx.stage) {
    case Stage::ReadCommand:
      if (!schedule(ctx, now, Stage::ReadSense)) terminal(ctx, MediaStatus::IntegrityError, now, false);
      return;
    case Stage::ReadSense: {
      const auto page = *topology_.flatten_page(ctx.command.address);
      const auto block = *topology_.flatten_block(ctx.command.address);
      const auto found = pages_.find(page);
      if (found == pages_.end()) { terminal(ctx, MediaStatus::ReadErasedPage, now, false); return; }
      ctx.read_snapshot = found->second.payload;
      const auto result = reliability_.evaluate_read(page, erase_counts_[block], 25.0);
      terminal(ctx, raw_status(result), now, false); return;
    }
    case Stage::ProgramData:
      if (!schedule(ctx, now, Stage::ProgramArray)) terminal(ctx, MediaStatus::IntegrityError, now, false);
      return;
    case Stage::ProgramArray:
      if (!schedule(ctx, now, Stage::ProgramVerify)) terminal(ctx, MediaStatus::IntegrityError, now, false);
      return;
    case Stage::ProgramVerify: {
      const auto block = *topology_.flatten_block(ctx.command.address);
      const bool fail = reliability_.fails(block, ras::ForcedFailure::Program);
      terminal(ctx, fail ? MediaStatus::ProgramFail : MediaStatus::Success, now, !fail); return;
    }
    case Stage::EraseSetup:
      if (!schedule(ctx, now, Stage::EraseArray)) terminal(ctx, MediaStatus::IntegrityError, now, false);
      return;
    case Stage::EraseArray:
      if (!schedule(ctx, now, Stage::EraseVerify)) terminal(ctx, MediaStatus::IntegrityError, now, false);
      return;
    case Stage::EraseVerify: {
      const auto block = *topology_.flatten_block(ctx.command.address);
      const bool fail = reliability_.fails(block, ras::ForcedFailure::Erase);
      terminal(ctx, fail ? MediaStatus::EraseFail : MediaStatus::Success, now, !fail); return;
    }
  }
}

void NandFlashDevice::terminal(Context context, MediaStatus status, Cycle now, bool commit) {
  const auto page = *topology_.flatten_page(context.command.address);
  const auto block = *topology_.flatten_block(context.command.address);
  if (commit && context.command.kind == CommandKind::Program) {
    PageRecord record;
    if (config_.payload_mode == PayloadMode::Sparse) record.payload = context.command.payload;
    else {
      for (auto byte : context.command.payload.bytes()) record.signature = record.signature * 131 + byte;
    }
    pages_[page] = std::move(record);
    ++committed_pages_;
  } else if (commit && context.command.kind == CommandKind::Erase) {
    const auto first = block * config_.geometry.pages_per_block;
    for (std::uint64_t i = 0; i < config_.geometry.pages_per_block; ++i) {
      if (pages_.erase(first + i)) --committed_pages_;
    }
    ++erase_counts_[block];
  }
  PayloadHandle result_payload;
  bool data_valid = false;
  if (context.command.kind == CommandKind::Read &&
      (status == MediaStatus::Success || status == MediaStatus::RawCorrectable ||
       status == MediaStatus::RefreshNotice)) {
    result_payload = context.read_snapshot;
    data_valid = config_.payload_mode == PayloadMode::Sparse;
  }
  contexts_.erase(context.command.token.value());
  completion_({context.command.token, status, data_valid, std::move(result_payload), now});
}

void NandFlashDevice::reset(Generation next, Cycle now) {
  std::vector<Context> aborted;
  for (const auto& item : contexts_) aborted.push_back(item.second);
  contexts_.clear();
  generation_ = next;
  events_.discard_generation(next);
  bank_eat_.clear(); die_eat_.clear();
  for (const auto& ctx : aborted)
    completion_({ctx.command.token, MediaStatus::Aborted, false, {}, now});
}
MediaSnapshot NandFlashDevice::snapshot() const {
  return {contexts_.size(), events_.snapshot().queued, committed_pages_, bbt_.size(), stale_events_};
}
bool NandFlashDevice::page_is_valid(const PhysicalAddress& a) const {
  const auto page = topology_.flatten_page(a); return page && pages_.count(*page);
}
std::uint64_t NandFlashDevice::block_erase_count(const PhysicalAddress& a) const {
  const auto block = topology_.flatten_block(a);
  const auto it = block ? erase_counts_.find(*block) : erase_counts_.end();
  return it == erase_counts_.end() ? 0 : it->second;
}
void NandFlashDevice::set_die_blocked(std::uint64_t die, bool blocked) {
  if (blocked) blocked_dies_.insert(die); else blocked_dies_.erase(die);
}
}  // namespace openhbx::media::nand
