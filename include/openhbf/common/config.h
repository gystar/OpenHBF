#pragma once

#include <cstdint>

#include "openhbf/common/geometry.h"

namespace openhbf::common {

constexpr std::uint32_t kConfigSchemaVersion = 1;

// Configuration shared by host/FTL/media. NAND timing and reliability remain
// owned by media and are intentionally absent here.
struct Config {
  Geometry geometry;
  ChannelOwnership ownership;
  std::uint32_t schema_version = kConfigSchemaVersion;
};

Result<void> validate(const Config& config);

// Compatibility adapter for old configuration objects that called the
// stack-wide count "core_dies_per_channel". New code should populate
// Geometry::core_die_count directly; this helper is the sole legacy bridge.
struct LegacyGeometry {
  std::uint16_t channels = 1;
  std::uint16_t core_dies_per_channel = 1;
  std::uint16_t dies_per_core = 1;
  std::uint16_t banks_per_die = 1;
  std::uint32_t blocks_per_bank = 1;
  std::uint32_t pages_per_block = 1;
  std::uint32_t bytes_per_page = 0;
};

Geometry from_legacy(const LegacyGeometry& legacy);

}  // namespace openhbf::common
