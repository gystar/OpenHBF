#pragma once

#include <cstdint>
#include <vector>

#include "openhbf/common/checked_math.h"
#include "openhbf/common/types.h"

namespace openhbf::common {

// Shared physical geometry.  core_die_count is a stack-wide count; channels
// are access owners and do not multiply physical capacity.
struct Geometry {
  std::uint16_t channels = 1;
  std::uint16_t core_die_count = 1;
  std::uint16_t dies_per_core = 1;
  std::uint16_t banks_per_die = 1;
  std::uint32_t blocks_per_bank = 1;
  std::uint32_t pages_per_block = 1;
  std::uint32_t bytes_per_page = 0;
};

struct ChannelOwnership {
  // Empty mapping selects the deterministic uniform profile (bank % channels).
  std::vector<ChannelId> bank_owner;

  Result<void> validate(const Geometry& geometry) const;
  Result<ChannelId> owner(const Geometry& geometry, std::uint16_t core,
                          std::uint16_t die, std::uint16_t bank) const;
};

Result<void> validate(const Geometry& geometry);
Result<std::uint64_t> physical_page_count(const Geometry& geometry);
Result<std::uint64_t> physical_capacity_bytes(const Geometry& geometry);


}  // namespace openhbf::common
