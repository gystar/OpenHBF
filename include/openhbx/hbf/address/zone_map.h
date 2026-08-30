#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "openhbx/hbf/address/hbf_address_types.h"

namespace openhbx::hbf::address {

enum class ZoneError { None, OutOfRange, Retired, Busy, SameZone };

struct ZoneLookup {
  BlockIndex physical_block;
  AddressViewEpoch epoch;
};

struct ZoneResult {
  ZoneError error{ZoneError::None};
  AddressViewEpoch epoch;
  explicit operator bool() const noexcept { return error == ZoneError::None; }
};

class ZoneMap {
 public:
  explicit ZoneMap(std::uint64_t zone_count);
  std::optional<ZoneLookup> lookup(ZoneId logical) const noexcept;
  std::optional<ZoneId> reverse(BlockIndex physical) const noexcept;
  ZoneResult remap(ZoneId first, ZoneId second, bool has_outstanding = false);
  ZoneResult retire(ZoneId logical, bool has_outstanding = false);
  bool retired(ZoneId logical) const noexcept;
  AddressViewEpoch epoch() const noexcept { return AddressViewEpoch(epoch_); }
  std::uint64_t payload_copy_events() const noexcept { return 0; }

 private:
  std::vector<std::uint64_t> logical_to_physical_;
  std::vector<bool> retired_;
  std::uint64_t epoch_{0};
};

}  // namespace openhbx::hbf::address
