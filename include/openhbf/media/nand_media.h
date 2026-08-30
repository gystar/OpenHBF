#pragma once

#include <memory>
#include <vector>

#include "openhbf/media/command_engine.h"
#include "openhbf/media/persistence.h"

namespace openhbf::media {

struct MediaSnapshot {
  TopologySnapshot topology;
  StateSnapshot state;
  std::size_t payload_bytes = 0;
  std::size_t in_flight = 0;
  BbtSnapshot bad_blocks;
  std::uint64_t retired_blocks = 0;
  DieEnvironmentSnapshot environment;
  MediaStatsSnapshot stats;
};

class NandMedia {
 public:
  static Result<std::unique_ptr<NandMedia>> create(
      MediaConfig config, CommandDependencies dependencies,
      std::vector<FaultRule> faults = {});

  IssueResult try_issue(const MediaCommand& command, Cycle now);
  void on_event(const StageEvent& event, Cycle now);
  void cancel_generation(Generation generation, Cycle now);
  bool idle() const noexcept;
  MediaSnapshot snapshot(Cycle now) const;
  Result<std::vector<std::uint8_t>> save_metadata() const;
  Result<void> restore_metadata(const std::vector<std::uint8_t>& bytes);
  Result<BlockState> block_state(const MediaAddress& address) const;

  BadBlockTable& bad_blocks() noexcept { return bbt_; }
  RetirementMap& retirement() noexcept { return retirement_; }
  DieEnvironment& environment() noexcept { return environment_; }
  MediaStats& stats() noexcept { return stats_; }

 private:
  NandMedia(MediaConfig config, MediaTopology topology,
            std::unique_ptr<IPageStore> pages, NandTimingModel timing,
            EatTable eat, ReliabilityModel reliability, BadBlockTable bbt,
            RetirementMap retirement,
            CommandDependencies dependencies);

  MediaConfig config_;
  MediaTopology topology_;
  std::unique_ptr<IPageStore> pages_;
  MediaStateStore state_;
  NandTimingModel timing_;
  EatTable eat_;
  ReliabilityModel reliability_;
  BadBlockTable bbt_;
  RetirementMap retirement_;
  DieEnvironment environment_;
  MediaStats stats_;
  CommandEngine engine_;
};

}  // namespace openhbf::media
