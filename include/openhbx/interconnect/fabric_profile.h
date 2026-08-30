#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "openhbx/interconnect/types.h"

namespace openhbx::interconnect {

enum class FabricTechnology { Tsv, TsvHybridBonding };

struct RouteProfile {
  RouteId id{0};
  std::uint64_t endpoint{0};
  std::vector<ResourceId> resources;
  std::vector<std::uint64_t> logical_lanes;
};

struct FabricProfile {
  FabricTechnology technology{FabricTechnology::Tsv};
  std::string source;
  std::size_t queue_depth{0};
  std::uint64_t channel_count{1};
  // Lane counts are per physical Channel, not aggregate cube counts.
  std::uint64_t active_lanes{0};
  std::uint64_t spare_lanes{0};
  std::uint64_t bits_per_lane_per_cycle{0};
  std::uint64_t efficiency_ppm{0};
  std::uint64_t arbitration_cycles{0};
  std::uint64_t propagation_cycles{0};
  std::vector<RouteProfile> routes;

  std::string validate() const;
  std::uint64_t effective_bits_per_cycle() const;
  std::uint64_t theoretical_bits_per_cycle() const;
  std::uint64_t total_active_lanes() const;
};

}  // namespace openhbx::interconnect
