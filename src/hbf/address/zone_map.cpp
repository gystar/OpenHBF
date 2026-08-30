#include "openhbx/hbf/address/zone_map.h"

#include <algorithm>
#include <numeric>

namespace openhbx::hbf::address {

ZoneMap::ZoneMap(std::uint64_t zone_count)
    : logical_to_physical_(zone_count), retired_(zone_count, false) {
  std::iota(logical_to_physical_.begin(), logical_to_physical_.end(), 0);
}

std::optional<ZoneLookup> ZoneMap::lookup(ZoneId logical) const noexcept {
  if (logical.value() >= logical_to_physical_.size() || retired_[logical.value()]) {
    return std::nullopt;
  }
  return ZoneLookup{BlockIndex(logical_to_physical_[logical.value()]),
                    AddressViewEpoch(epoch_)};
}

std::optional<ZoneId> ZoneMap::reverse(BlockIndex physical) const noexcept {
  const auto it = std::find(logical_to_physical_.begin(), logical_to_physical_.end(),
                            physical.value());
  if (it == logical_to_physical_.end()) return std::nullopt;
  const auto index = static_cast<std::uint64_t>(
      std::distance(logical_to_physical_.begin(), it));
  if (retired_[index]) return std::nullopt;
  return ZoneId(index);
}

ZoneResult ZoneMap::remap(ZoneId first, ZoneId second, bool has_outstanding) {
  if (first.value() >= logical_to_physical_.size() ||
      second.value() >= logical_to_physical_.size()) return {ZoneError::OutOfRange, epoch()};
  if (first == second) return {ZoneError::SameZone, epoch()};
  if (retired_[first.value()] || retired_[second.value()]) return {ZoneError::Retired, epoch()};
  if (has_outstanding) return {ZoneError::Busy, epoch()};
  std::swap(logical_to_physical_[first.value()], logical_to_physical_[second.value()]);
  ++epoch_;
  return {ZoneError::None, epoch()};
}

ZoneResult ZoneMap::retire(ZoneId logical, bool has_outstanding) {
  if (logical.value() >= logical_to_physical_.size()) return {ZoneError::OutOfRange, epoch()};
  if (retired_[logical.value()]) return {ZoneError::Retired, epoch()};
  if (has_outstanding) return {ZoneError::Busy, epoch()};
  retired_[logical.value()] = true;
  ++epoch_;
  return {ZoneError::None, epoch()};
}

bool ZoneMap::retired(ZoneId logical) const noexcept {
  return logical.value() >= retired_.size() || retired_[logical.value()];
}

}  // namespace openhbx::hbf::address
