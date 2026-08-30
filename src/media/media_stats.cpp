#include "openhbf/media/media_stats.h"

#include <limits>
#include <utility>

namespace openhbf::media {
namespace {

std::uint64_t payload_bytes(MediaOp op) noexcept {
  return op == MediaOp::Erase ? 0 : kPageBytes;
}

void saturating_add(std::uint64_t& target, std::uint64_t value) noexcept {
  const std::uint64_t available = std::numeric_limits<std::uint64_t>::max() - target;
  target += value > available ? available : value;
}

}  // namespace

MediaStats::MediaStats(std::size_t log_capacity, bool log_enabled)
    : log_capacity_(log_capacity), log_enabled_(log_enabled) {}

DirectionStats& MediaStats::direction(MediaOp op) noexcept {
  switch (op) {
    case MediaOp::Read: return counters_.read;
    case MediaOp::Program: return counters_.program;
    case MediaOp::Erase: return counters_.erase;
  }
  return counters_.read;
}

void MediaStats::append(MediaLogRecord record) {
  record.sequence = next_sequence_++;
  if (!log_enabled_) return;
  if (log_capacity_ == 0 || log_.size() == log_capacity_) {
    saturating_add(counters_.dropped_log_records, 1);
    return;
  }
  log_.push_back(std::move(record));
}

void MediaStats::on_issue(SubmitState state, MediaToken token,
                          const MediaCommand& command, Cycle cycle,
                          MediaStatus result) {
  switch (state) {
    case SubmitState::Accepted:
      saturating_add(counters_.accepted, 1);
      saturating_add(direction(command.op).commands, 1);
      break;
    case SubmitState::Busy: saturating_add(counters_.busy, 1); break;
    case SubmitState::Rejected: saturating_add(counters_.rejected, 1); break;
  }
  append(MediaLogRecord{cycle, 0, token, command.generation, command.address,
                        command.op, StageKind::ReadSense, LogEvent::Issue,
                        result, 0, cycle, cycle, {}});
}

void MediaStats::on_stage(MediaToken token, Generation generation,
                          const MediaAddress& address, MediaOp op,
                          StageKind stage, Cycle start, Cycle end,
                          std::vector<ResourceId> resources) {
  const std::uint64_t elapsed = end.value() >= start.value()
      ? end.value() - start.value() : 0;
  saturating_add(counters_.stage_cycles, elapsed);
  append(MediaLogRecord{end, 0, token, generation, address, op, stage,
                        LogEvent::Stage, MediaStatus::Success, 0, start, end,
                        std::move(resources)});
}

void MediaStats::on_complete(const MediaCompletion& completion,
                             std::uint64_t retry_bytes,
                             std::uint64_t failure_bytes) {
  saturating_add(counters_.terminal, 1);
  saturating_add(counters_.raw_errors, completion.raw_errors);
  DirectionStats& stats = direction(completion.op);
  const std::uint64_t bytes = payload_bytes(completion.op);
  if (completion.status == MediaStatus::Success) {
    saturating_add(counters_.successful, 1);
    saturating_add(stats.successful_bytes, bytes);
  } else if (completion.status == MediaStatus::RetryRequired) {
    saturating_add(counters_.retry, 1);
    saturating_add(stats.retry_bytes, retry_bytes == 0 ? bytes : retry_bytes);
  } else {
    saturating_add(counters_.failed, 1);
    saturating_add(stats.failed_bytes, failure_bytes == 0 ? bytes : failure_bytes);
  }
  append(MediaLogRecord{completion.completed_cycle, 0, completion.token,
                        completion.generation, completion.address,
                        completion.op, StageKind::ReadSense, LogEvent::Complete,
                        completion.status,
                        completion.status == MediaStatus::Success ? bytes : 0,
                        completion.accepted_cycle, completion.completed_cycle, {}});
}

MediaStatsSnapshot MediaStats::snapshot(Cycle now) const noexcept {
  MediaStatsSnapshot copy = counters_;
  copy.observed_at = now;
  return copy;
}

std::vector<MediaLogRecord> MediaStats::drain_log() {
  std::vector<MediaLogRecord> result;
  result.reserve(log_.size());
  while (!log_.empty()) {
    result.push_back(std::move(log_.front()));
    log_.pop_front();
  }
  return result;
}

}  // namespace openhbf::media
