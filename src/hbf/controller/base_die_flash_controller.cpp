#include "openhbx/hbf/controller/base_die_flash_controller.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace openhbx::hbf::controller {
namespace {
Cycle deadline(Cycle now, std::uint64_t timeout) {
  if (timeout == 0 || now.value() > std::numeric_limits<std::uint64_t>::max() - timeout)
    return Cycle(std::numeric_limits<std::uint64_t>::max());
  return Cycle(now.value() + timeout);
}
PayloadHandle sector_slice(const PayloadHandle& page, address::SectorIndex sector) {
  if (page.size() != 4096 || sector.value() >= 64) return {};
  const auto begin = page.bytes().begin() + static_cast<std::ptrdiff_t>(sector.value() * 64);
  return PayloadHandle::from_bytes(std::vector<std::uint8_t>(begin, begin + 64));
}
}  // namespace

BaseDieFlashController::BaseDieFlashController(ControllerConfig config,
    std::vector<address::PhysicalBank> banks, address::BlockSequence& sequences,
    pal::FlashPal& pal, ControllerCompletionSink completion)
    : config_(config), sequences_(sequences), pal_(pal), completion_(std::move(completion)),
      accumulator_(config.max_pending_dlu, config.accumulation_timeout_cycles),
      scheduler_(config.queue_depth), cache_(std::move(banks), config.cache_buffers_per_bank),
      ecc_(config.ecc_credits), control_(config.scratchpad_bytes) {
  if (config.max_pending_dlu == 0 || config.accumulation_timeout_cycles == 0 ||
      config.queue_depth == 0 || config.ecc_credits == 0)
    throw std::invalid_argument("controller resources must be nonzero");
  if (config_.backend_wait_timeout_cycles == 0)
    config_.backend_wait_timeout_cycles = config_.accumulation_timeout_cycles;
}

AdmissionResult BaseDieFlashController::submit(ControllerRequest request, Cycle now) {
  if (request.token.value() == 0 || request.generation != generation_)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  if ((request.operation == HostOperation::WriteSector ||
       request.operation == HostOperation::ReadSector) && request.sector.value() >= 64)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument, "sector must be below 64");
  if (!seen_host_tokens_.insert(request.token.value()).second)
    return AdmissionResult::rejected(RejectionReason::InvalidLifecycle, "duplicate Host token");
  AdmissionResult result = request.operation == HostOperation::WriteSector
      ? submit_write(std::move(request), now) : submit_read(std::move(request), now);
  if (result.code != AdmissionCode::Accepted) seen_host_tokens_.erase(request.token.value());
  return result;
}

AdmissionResult BaseDieFlashController::submit_admin(Token token, Generation generation,
    AdminOpcode opcode, bool vendor_enabled, Cycle now) {
  if (token.value() == 0 || generation != generation_ ||
      !seen_host_tokens_.insert(token.value()).second)
    return AdmissionResult::rejected(RejectionReason::InvalidLifecycle);
  ++accepted_;
  const auto result = control_.validate(opcode, vendor_enabled);
  terminal(token, result == AdminResult::Accepted ? ControllerStatus::Unsupported
                                                   : ControllerStatus::Unsupported,
           false, {}, now);
  return AdmissionResult::accepted();
}

AdmissionResult BaseDieFlashController::submit_write(ControllerRequest request, Cycle now) {
  const auto result = accumulator_.add_sector(request.dlu, request.sector,
                                               request.payload, request.token, now);
  if (result.code == AccumulateCode::Overlap || result.code == AccumulateCode::Limit) {
    ++accepted_; terminal(request.token, result.code == AccumulateCode::Overlap
        ? ControllerStatus::Overlap : ControllerStatus::PendingLimit, false, {}, now);
    return AdmissionResult::accepted();
  }
  if (result.code == AccumulateCode::Invalid)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  ++accepted_;
  if (result.code != AccumulateCode::Complete) return AdmissionResult::accepted();

  const Token reservation_token(next_work_token_++);
  auto reserved = sequences_.reserve_program(request.dlu.block, request.dlu.page, reservation_token);
  if (!reserved) {
    for (auto token : accumulator_.tokens(request.dlu))
      terminal(token, map_sequence(reserved.error), false, {}, now);
    accumulator_.release(request.dlu); return AdmissionResult::accepted();
  }
  BackendRecord record;
  record.request = request;
  record.stage = reserved.auto_erase_required ? BackendRecord::Stage::Erase
                                               : BackendRecord::Stage::Program;
  record.program_payload = result.payload; record.host_tokens = accumulator_.tokens(request.dlu);
  record.reservation = *reserved.reservation; record.work_token = Token(next_work_token_++);
  record.deadline = deadline(now, config_.backend_wait_timeout_cycles);
  if (!enqueue(std::move(record), WorkKind::Program)) {
    sequences_.cancel_program(*reserved.reservation);
    for (auto token : accumulator_.tokens(request.dlu))
      terminal(token, ControllerStatus::ProgramFail, false, {}, now);
    accumulator_.release(request.dlu);
  }
  drive_scheduler(now);
  return AdmissionResult::accepted();
}

AdmissionResult BaseDieFlashController::submit_read(ControllerRequest request, Cycle now) {
  if (request.operation == HostOperation::ReadSector) {
    const auto probe = accumulator_.probe_read(request.dlu, request.sector);
    if (probe.code != ProbeCode::Miss) {
      ++accepted_;
      terminal(request.token, probe.code == ProbeCode::Forward ? ControllerStatus::Success
                                                               : ControllerStatus::PendingDataMissing,
               probe.code == ProbeCode::Forward, probe.payload, now);
      return AdmissionResult::accepted();
    }
  }
  if (auto hit = cache_.lookup(request.bank, request.dlu)) {
    ++accepted_;
    auto payload = request.operation == HostOperation::ReadSector
        ? sector_slice(hit->payload, request.sector) : hit->payload;
    terminal(request.token, payload.empty() ? ControllerStatus::Invalid : ControllerStatus::Success,
             !payload.empty(), payload, now);
    return AdmissionResult::accepted();
  }
  auto cache = cache_.reserve(request.bank, request.dlu);
  if (!cache) return AdmissionResult::busy();
  if (ecc_inflight_ >= ecc_.credits()) { cache_.release(*cache); return AdmissionResult::busy(); }
  BackendRecord record; record.request = request; record.cache = cache;
  record.stage = BackendRecord::Stage::Read; record.work_token = Token(next_work_token_++);
  record.deadline = deadline(now, config_.backend_wait_timeout_cycles);
  if (!enqueue(std::move(record), WorkKind::Read)) { cache_.release(*cache); return AdmissionResult::busy(); }
  ++ecc_inflight_; ++accepted_; drive_scheduler(now); return AdmissionResult::accepted();
}

bool BaseDieFlashController::enqueue(BackendRecord record, WorkKind kind) {
  const auto work_token = record.work_token;
  if (!scheduler_.enqueue({work_token, record.request.bank, kind,
                           record.request.read_mode, 0})) return false;
  ready_.emplace(work_token.value(), std::move(record)); return true;
}

void BaseDieFlashController::drive_scheduler(Cycle now) {
  while (auto work = scheduler_.select()) {
    auto found = ready_.find(work->token.value());
    if (found == ready_.end()) { scheduler_.complete(work->bank); continue; }
    auto& record = found->second;
    const Token pal_token(next_pal_token_++);
    media::CommandKind kind = media::CommandKind::Read;
    PayloadHandle payload;
    if (record.stage == BackendRecord::Stage::Erase) kind = media::CommandKind::Erase;
    if (record.stage == BackendRecord::Stage::Program) {
      kind = media::CommandKind::Program; payload = record.program_payload;
    }
    const auto admission = pal_.try_issue({pal_token, generation_, kind,
        physical_address(record.request), record.request.endpoint, payload}, now);
    if (admission.code == AdmissionCode::Accepted) {
      backend_.emplace(pal_token.value(), std::move(record)); ready_.erase(found);
      continue;
    }
    scheduler_.complete(work->bank);
    if (admission.code == AdmissionCode::Busy) {
      scheduler_.enqueue(*work); break;
    }
    auto failed = std::move(record); ready_.erase(found);
    if (failed.reservation) sequences_.cancel_program(*failed.reservation);
    if (failed.cache) { cache_.release(*failed.cache); if (ecc_inflight_ != 0) --ecc_inflight_; }
    if (!failed.host_tokens.empty()) {
      for (auto token : failed.host_tokens) terminal(token, ControllerStatus::ProgramFail, false, {}, now);
      accumulator_.release(failed.request.dlu);
    } else terminal(failed.request.token, ControllerStatus::UnsupportedSpecGap, false, {}, now);
  }
}

void BaseDieFlashController::pump(Cycle now) {
  for (const auto& expired : accumulator_.expire(now))
    for (auto token : expired.tokens) terminal(token, ControllerStatus::AccumulationTimeout, false, {}, now);
  for (auto it = ready_.begin(); it != ready_.end();) {
    if (it->second.deadline > now) { ++it; continue; }
    auto record = std::move(it->second); scheduler_.cancel(record.work_token); it = ready_.erase(it);
    if (record.reservation) sequences_.cancel_program(*record.reservation);
    if (record.cache) { cache_.release(*record.cache); if (ecc_inflight_ != 0) --ecc_inflight_; }
    if (!record.host_tokens.empty()) {
      for (auto token : record.host_tokens) terminal(token, ControllerStatus::ProgramFail, false, {}, now);
      accumulator_.release(record.request.dlu);
    } else terminal(record.request.token, ControllerStatus::Aborted, false, {}, now);
  }
  drive_scheduler(now);
}

void BaseDieFlashController::on_pal_completion(pal::PalCompletion completion) {
  auto it = backend_.find(completion.token.value());
  if (it == backend_.end()) return;
  auto record = std::move(it->second); backend_.erase(it);
  scheduler_.complete(record.request.bank);
  if (record.stage == BackendRecord::Stage::Erase) {
    if (completion.status == pal::PalStatus::Success) {
      record.stage = BackendRecord::Stage::Program; record.work_token = Token(next_work_token_++);
      const auto reservation = record.reservation;
      const auto dlu = record.request.dlu;
      const auto tokens = record.host_tokens;
      if (!enqueue(std::move(record), WorkKind::Program)) {
        sequences_.cancel_program(*reservation);
        for (auto token : tokens)
          terminal(token, ControllerStatus::ProgramFail, false, {}, completion.completed_at);
        accumulator_.release(dlu);
      }
      drive_scheduler(completion.completed_at); return;
    }
    sequences_.complete_program(*record.reservation, address::ProgramResult::Failure);
    for (auto token : record.host_tokens) terminal(token, ControllerStatus::ProgramFail, false, {}, completion.completed_at);
    accumulator_.release(record.request.dlu); drive_scheduler(completion.completed_at); return;
  }
  if (record.stage == BackendRecord::Stage::Program) {
    const bool success = completion.status == pal::PalStatus::Success;
    sequences_.complete_program(*record.reservation, success ? address::ProgramResult::Success
                                                             : address::ProgramResult::Failure);
    auto status = success ? ControllerStatus::Success : ControllerStatus::ProgramFail;
    if (completion.status == pal::PalStatus::CapacityUnusable) status = ControllerStatus::CapacityUnusable;
    if (completion.status == pal::PalStatus::DieTemporarilyBlocked) status = ControllerStatus::DieTemporarilyBlocked;
    for (auto token : record.host_tokens) terminal(token, status, false, {}, completion.completed_at);
    accumulator_.release(record.request.dlu); drive_scheduler(completion.completed_at); return;
  }
  const auto decoded = ecc_.decode(completion);
  if (ecc_inflight_ != 0) --ecc_inflight_;
  if (record.cache) {
    if (decoded.data_valid) cache_.fill(*record.cache, decoded.payload); else cache_.release(*record.cache);
  }
  auto payload = decoded.payload;
  if (decoded.data_valid && record.request.operation == HostOperation::ReadSector)
    payload = sector_slice(decoded.payload, record.request.sector);
  const bool valid = decoded.data_valid && !payload.empty();
  const auto status = decoded.status == ControllerStatus::Success && !valid
      ? ControllerStatus::Invalid : decoded.status;
  terminal(record.request.token, status, valid, payload,
           completion.completed_at, decoded.error_info);
  drive_scheduler(completion.completed_at);
}

void BaseDieFlashController::reset(Generation next, Cycle now) {
  if (next <= generation_) return;
  for (auto& pair : ready_) {
    auto& r = pair.second; if (r.reservation) sequences_.cancel_program(*r.reservation);
    if (r.cache) cache_.release(*r.cache);
    if (!r.host_tokens.empty()) for (auto t : r.host_tokens) terminal(t, ControllerStatus::Aborted, false, {}, now);
    else terminal(r.request.token, ControllerStatus::Aborted, false, {}, now);
    if (!r.host_tokens.empty()) accumulator_.release(r.request.dlu);
  }
  for (auto& pair : backend_) {
    auto& r = pair.second; if (r.reservation) sequences_.cancel_program(*r.reservation);
    if (!r.host_tokens.empty()) for (auto t : r.host_tokens) terminal(t, ControllerStatus::Aborted, false, {}, now);
    else terminal(r.request.token, ControllerStatus::Aborted, false, {}, now);
    if (!r.host_tokens.empty()) accumulator_.release(r.request.dlu);
  }
  for (const auto& partial : accumulator_.expire(Cycle(~std::uint64_t{0})))
    for (auto token : partial.tokens) terminal(token, ControllerStatus::Aborted, false, {}, now);
  ready_.clear(); backend_.clear(); accumulator_.reset(); scheduler_.reset(); cache_.reset();
  ecc_inflight_ = 0;
  control_.reset(true); pal_.reset(next, now); generation_ = next; seen_host_tokens_.clear();
}

void BaseDieFlashController::terminal(Token token, ControllerStatus status, bool valid,
    PayloadHandle payload, Cycle now, ControllerErrorInfo info) {
  ++terminal_;
  completion_({token, generation_, status, valid,
               valid ? std::move(payload) : PayloadHandle{}, now, info});
}
ControllerStatus BaseDieFlashController::map_sequence(address::SequenceError error) {
  return error == address::SequenceError::Retired ? ControllerStatus::CapacityUnusable
                                                   : ControllerStatus::OrderViolation;
}
ControllerSnapshot BaseDieFlashController::snapshot() const {
  return {accepted_, terminal_, accepted_ - terminal_, ready_.size()};
}
}  // namespace openhbx::hbf::controller
