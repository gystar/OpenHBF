#include "openhbx/interconnect/tsv_repair.h"

#include <stdexcept>

#include "openhbx/common/checked_math.h"

namespace openhbx::interconnect {

TsvRepairManager::TsvRepairManager(std::uint64_t active, std::uint64_t spares)
    : TsvRepairManager(1, active, spares) {}

TsvRepairManager::TsvRepairManager(std::uint64_t channels,
                                   std::uint64_t active,
                                   std::uint64_t spares)
    : active_lanes_per_channel_(active) {
  const auto physical_stride = checked_add(active, spares);
  const auto total_physical = physical_stride
      ? checked_mul(channels, *physical_stride) : std::nullopt;
  const auto total_logical = checked_mul(channels, active);
  if (channels == 0 || active == 0 || !physical_stride || !total_physical ||
      !total_logical)
    throw std::invalid_argument("TSV lane count overflow");
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    for (std::uint64_t lane = 0; lane < active; ++lane) {
      const std::uint64_t logical = channel * active + lane;
      const std::uint64_t physical = channel * *physical_stride + lane;
      active_to_physical_[logical] = physical;
    }
    for (std::uint64_t spare = 0; spare < spares; ++spare) {
      const std::uint64_t physical = channel * *physical_stride + active + spare;
      free_spares_.insert(physical);
      free_spares_by_channel_[channel].insert(physical);
    }
  }
  epoch_maps_.emplace(epoch_, active_to_physical_);
}

RepairResult TsvRepairManager::apply_fault(std::uint64_t physical) {
  std::uint64_t logical = 0;
  bool found = false;
  for (const auto& item : active_to_physical_) {
    if (item.second == physical) { logical = item.first; found = true; break; }
  }
  if (!found) {
    if (failed_physical_.count(physical)) return {RepairCode::AlreadyFailed, epoch_, 0};
    return {RepairCode::UnknownLane, epoch_, 0};
  }
  failed_physical_.insert(physical);
  ++epoch_;
  const std::uint64_t channel = logical / active_lanes_per_channel_;
  auto& channel_spares = free_spares_by_channel_[channel];
  if (channel_spares.empty()) {
    active_to_physical_.erase(logical);
    epoch_maps_[epoch_] = active_to_physical_;
    return {RepairCode::Unavailable, epoch_, 0};
  }
  const auto spare = *channel_spares.begin();
  channel_spares.erase(channel_spares.begin());
  free_spares_.erase(spare);
  active_to_physical_[logical] = spare;
  epoch_maps_[epoch_] = active_to_physical_;
  return {RepairCode::Repaired, epoch_, spare};
}

bool TsvRepairManager::resolve(const RouteProfile& profile, Route& route) const {
  for (auto logical : profile.logical_lanes)
    if (active_to_physical_.find(logical) == active_to_physical_.end()) return false;
  route = {profile.id, profile.resources, profile.logical_lanes, {}, epoch_};
  route.physical_lanes.reserve(profile.logical_lanes.size());
  for (auto logical : profile.logical_lanes)
    route.physical_lanes.push_back(active_to_physical_.at(logical));
  return true;
}

bool TsvRepairManager::is_operational(const Route& route) const {
  if (route.logical_lanes.size() != route.physical_lanes.size()) return false;
  const auto epoch = epoch_maps_.find(route.repair_epoch);
  if (epoch == epoch_maps_.end()) return false;
  for (std::size_t index = 0; index < route.physical_lanes.size(); ++index) {
    const auto logical = epoch->second.find(route.logical_lanes[index]);
    if (logical == epoch->second.end() || logical->second != route.physical_lanes[index])
      return false;
    const auto physical = route.physical_lanes[index];
    if (failed_physical_.count(physical) != 0) return false;
  }
  return true;
}

RepairSnapshot TsvRepairManager::snapshot() const {
  return {epoch_, active_to_physical_, failed_physical_, free_spares_};
}

}  // namespace openhbx::interconnect
