#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "openhbf/media/types.h"

namespace openhbf::media {

enum class LogEvent : std::uint8_t { Issue, Stage, Complete };

struct MediaLogRecord {
  Cycle cycle{};
  std::uint64_t sequence = 0;
  MediaToken token{};
  Generation generation{};
  MediaAddress address;
  MediaOp op = MediaOp::Read;
  StageKind stage = StageKind::ReadSense;
  LogEvent event = LogEvent::Issue;
  MediaStatus result = MediaStatus::Success;
  std::uint64_t bytes = 0;
  Cycle start_cycle{};
  Cycle end_cycle{};
  std::vector<ResourceId> resources;
};

struct DirectionStats {
  std::uint64_t commands = 0;
  std::uint64_t successful_bytes = 0;
  std::uint64_t failed_bytes = 0;
  std::uint64_t retry_bytes = 0;
};

struct MediaStatsSnapshot {
  Cycle observed_at{};
  std::uint64_t accepted = 0;
  std::uint64_t busy = 0;
  std::uint64_t rejected = 0;
  std::uint64_t terminal = 0;
  std::uint64_t successful = 0;
  std::uint64_t failed = 0;
  std::uint64_t retry = 0;
  std::uint64_t raw_errors = 0;
  std::uint64_t stage_cycles = 0;
  std::uint64_t dropped_log_records = 0;
  DirectionStats read;
  DirectionStats program;
  DirectionStats erase;
};

class MediaStats {
 public:
  explicit MediaStats(std::size_t log_capacity, bool log_enabled = true);

  void on_issue(SubmitState state, MediaToken token, const MediaCommand& command,
                Cycle cycle, MediaStatus result);
  void on_stage(MediaToken token, Generation generation,
                const MediaAddress& address, MediaOp op, StageKind stage,
                Cycle start, Cycle end,
                std::vector<ResourceId> resources = {});
  void on_complete(const MediaCompletion& completion,
                   std::uint64_t retry_bytes = 0,
                   std::uint64_t failure_bytes = 0);

  MediaStatsSnapshot snapshot(Cycle now) const noexcept;
  std::vector<MediaLogRecord> drain_log();
  void set_log_enabled(bool enabled) noexcept { log_enabled_ = enabled; }

 private:
  DirectionStats& direction(MediaOp op) noexcept;
  void append(MediaLogRecord record);

  MediaStatsSnapshot counters_;
  std::size_t log_capacity_ = 0;
  bool log_enabled_ = true;
  std::uint64_t next_sequence_ = 0;
  std::deque<MediaLogRecord> log_;
};

}  // namespace openhbf::media
