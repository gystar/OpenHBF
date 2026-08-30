#include "openhbx/interconnect/fabric_profile.h"

#include <limits>
#include <set>

#include "openhbx/common/checked_math.h"

namespace openhbx::interconnect {

std::string FabricProfile::validate() const {
  if (source.empty()) return "profile source is required";
  if (queue_depth == 0) return "queue_depth must be non-zero";
  if (channel_count == 0 || active_lanes == 0 || bits_per_lane_per_cycle == 0)
    return "channel and lane bandwidth must be non-zero";
  if (efficiency_ppm == 0 || efficiency_ppm > 1000000) return "efficiency_ppm is out of range";
  if (technology == FabricTechnology::TsvHybridBonding && source.find("vendor:") != 0)
    return "hybrid bonding requires a vendor source";
  if (!checked_add(active_lanes, spare_lanes) || total_active_lanes() == 0)
    return "lane count overflow";
  const auto raw = checked_mul(active_lanes, bits_per_lane_per_cycle);
  if (!raw) return "raw bandwidth overflow";
  const auto scaled = checked_mul(*raw, efficiency_ppm);
  if (!scaled) return "effective bandwidth overflow";
  if (*scaled / 1000000 == 0) return "effective bandwidth must be non-zero";
  std::set<RouteId> ids;
  std::set<std::uint64_t> endpoints;
  for (const auto& route : routes) {
    if (route.id == 0 || route.resources.empty() || route.logical_lanes.empty())
      return "route must have an id, resources, and lanes";
    if (!ids.insert(route.id).second || !endpoints.insert(route.endpoint).second)
      return "route id and endpoint must be unique";
    for (auto lane : route.logical_lanes)
      if (lane >= total_active_lanes()) return "route references an unknown active lane";
    std::set<ResourceId> resources(route.resources.begin(), route.resources.end());
    std::set<std::uint64_t> lanes(route.logical_lanes.begin(), route.logical_lanes.end());
    if (resources.size() != route.resources.size() || lanes.size() != route.logical_lanes.size())
      return "route resources and lanes must be unique";
  }
  return {};
}

std::uint64_t FabricProfile::theoretical_bits_per_cycle() const {
  const auto raw = checked_mul(active_lanes, bits_per_lane_per_cycle);
  if (!raw) return 0;
  const auto scaled = checked_mul(*raw, efficiency_ppm);
  return scaled ? *scaled / 1000000 : 0;
}

std::uint64_t FabricProfile::effective_bits_per_cycle() const {
  return theoretical_bits_per_cycle();
}

std::uint64_t FabricProfile::total_active_lanes() const {
  const auto total = checked_mul(channel_count, active_lanes);
  return total ? *total : 0;
}

}  // namespace openhbx::interconnect
