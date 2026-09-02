#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "openhbx/common/admission.h"
#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/media/media_snapshot.h"
#include "openhbx/media/nand/topology.h"
#include "openhbx/ras/bad_block_table.h"
#include "openhbx/ras/reliability_model.h"
#include "openhbx/system/event_queue.h"

namespace openhbx::media::nand {

enum class PayloadMode { Sparse, TimingOnly };
struct StageTiming {
  // Fast component-test defaults. Product timings come from ResolvedSystemModel.
  std::uint64_t read_command{1}, read_sense{10};
  std::uint64_t program_data_in{2}, program_array{20}, program_verify{3};
  std::uint64_t erase_setup{1}, erase_array{40}, erase_verify{4};
};
struct NandDeviceConfig {
  config::HbfGeometry geometry;
  PayloadMode payload_mode{PayloadMode::Sparse};
  StageTiming timing;
  ras::ReliabilityProfile reliability;
  std::size_t max_inflight{64};
  bool planes_enabled{false};
  std::uint64_t planes_per_die{1};
  bool multi_plane_enabled{false};
};

class NandFlashDevice {
 public:
  NandFlashDevice(NandDeviceConfig config, EventQueue& events, HandlerId handler,
                  CompletionSink completion);
  AdmissionResult try_issue(FlashCommand command, Cycle now);
  void reset(Generation next_generation, Cycle now);
  MediaSnapshot snapshot() const;
  const MediaTopology& topology() const noexcept { return topology_; }
  bool page_is_valid(const PhysicalAddress& address) const;
  std::uint64_t block_erase_count(const PhysicalAddress& address) const;
  ras::BadBlockTable& bad_block_table() noexcept { return bbt_; }
  ras::ReliabilityModel& reliability() noexcept { return reliability_; }
  void set_die_blocked(std::uint64_t die, bool blocked);

 private:
  enum class Stage : std::uint8_t { ReadCommand, ReadSense, ProgramData, ProgramArray,
                                    ProgramVerify, EraseSetup, EraseArray, EraseVerify };
  struct Context { FlashCommand command; Stage stage; Cycle due; std::uint64_t sequence{0}; PayloadHandle read_snapshot; };
  struct PageRecord { PayloadHandle payload; std::uint64_t signature{0}; };
  struct Reservation { Cycle end; };
  void on_event(EventPayload payload);
  bool schedule(Context& context, Cycle now, Stage next);
  std::uint64_t duration(Stage stage) const;
  bool uses_die(Stage stage) const;
  void terminal(Context context, MediaStatus status, Cycle now, bool commit);
  MediaStatus validate(const FlashCommand& command) const;

  NandDeviceConfig config_;
  MediaTopology topology_;
  EventQueue& events_;
  HandlerId handler_;
  CompletionSink completion_;
  Generation generation_{0};
  std::map<std::uint64_t, Context> contexts_;
  std::map<std::uint64_t, PageRecord> pages_;
  std::map<std::uint64_t, std::uint64_t> erase_counts_;
  std::map<std::uint64_t, Cycle> bank_eat_, die_eat_;
  std::set<std::uint64_t> blocked_dies_;
  ras::BadBlockTable bbt_;
  ras::ReliabilityModel reliability_;
  std::uint64_t committed_pages_{0}, stale_events_{0};
};

}  // namespace openhbx::media::nand
