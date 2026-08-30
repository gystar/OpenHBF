#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "openhbx/interconnect/fabric_profile.h"

namespace openhbx::interconnect {

enum class RepairCode { Repaired, AlreadyFailed, UnknownLane, Unavailable };

struct RepairResult {
  RepairCode code{RepairCode::UnknownLane};
  std::uint64_t epoch{0};
  std::uint64_t replacement{0};
};

struct RepairSnapshot {
  std::uint64_t epoch{0};
  std::map<std::uint64_t, std::uint64_t> active_to_physical;
  std::set<std::uint64_t> failed_physical;
  std::set<std::uint64_t> free_spares;
};

class TsvRepairManager {
 public:
  TsvRepairManager(std::uint64_t active_lanes, std::uint64_t spare_lanes);
  TsvRepairManager(std::uint64_t channel_count,
                   std::uint64_t active_lanes_per_channel,
                   std::uint64_t spare_lanes_per_channel);
  RepairResult apply_fault(std::uint64_t physical_lane);
  bool resolve(const RouteProfile& profile, Route& route) const;
  bool is_operational(const Route& route) const;
  RepairSnapshot snapshot() const;

 private:
  std::uint64_t epoch_{0};
  std::map<std::uint64_t, std::uint64_t> active_to_physical_;
  std::set<std::uint64_t> failed_physical_;
  std::set<std::uint64_t> free_spares_;
  std::map<std::uint64_t, std::set<std::uint64_t>> free_spares_by_channel_;
  std::uint64_t active_lanes_per_channel_{0};
  std::map<std::uint64_t, std::map<std::uint64_t, std::uint64_t>> epoch_maps_;
};

}  // namespace openhbx::interconnect
