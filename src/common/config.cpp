#include "openhbf/common/config.h"

#include <string>

namespace openhbf::common {

Result<void> validate(const Geometry& g) {
  if (g.channels == 0 || g.channels > 16 || g.core_die_count == 0 || g.dies_per_core == 0 ||
      g.banks_per_die == 0 || g.blocks_per_bank == 0 ||
      g.pages_per_block == 0 || g.bytes_per_page == 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "geometry dimensions must be non-zero"});
  }
  return Result<void>::success();
}

Result<void> ChannelOwnership::validate(const Geometry& g) const {
  auto valid = common::validate(g);
  if (!valid) return valid;
  auto dies_banks = checked_mul<std::uint64_t>(
      static_cast<std::uint64_t>(g.dies_per_core), g.banks_per_die);
  if (!dies_banks) return Result<void>::failure(dies_banks.error());
  auto banks = checked_mul<std::uint64_t>(g.core_die_count,
                                          dies_banks.value());
  if (!banks) return Result<void>::failure(banks.error());
  if (!bank_owner.empty() && bank_owner.size() != banks.value()) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "channel ownership mapping has wrong length"});
  }
  for (auto owner : bank_owner) {
    if (owner.value() >= g.channels)
      return Result<void>::failure(
          {ErrorCode::kOutOfRange, "channel ownership references unknown channel"});
  }
  return Result<void>::success();
}

Result<ChannelId> ChannelOwnership::owner(const Geometry& g, std::uint16_t core,
                                          std::uint16_t die,
                                          std::uint16_t bank) const {
  auto valid = validate(g);
  if (!valid) return Result<ChannelId>::failure(valid.error());
  if (core >= g.core_die_count || die >= g.dies_per_core || bank >= g.banks_per_die)
    return Result<ChannelId>::failure({ErrorCode::kOutOfRange, "bank coordinate out of range"});
  const auto index = (static_cast<std::uint64_t>(core) * g.dies_per_core + die) *
                     g.banks_per_die + bank;
  return Result<ChannelId>::success(bank_owner.empty()
                                        ? ChannelId(static_cast<std::uint16_t>(bank % g.channels))
                                        : bank_owner[index]);
}

Result<std::uint64_t> physical_page_count(const Geometry& g) {
  auto valid = validate(g); if (!valid) return Result<std::uint64_t>::failure(valid.error());
  auto a = checked_mul<std::uint64_t>(g.core_die_count, g.dies_per_core);
  if (!a) return Result<std::uint64_t>::failure(a.error());
  auto b = checked_mul<std::uint64_t>(a.value(), g.banks_per_die); if (!b) return Result<std::uint64_t>::failure(b.error());
  auto c = checked_mul<std::uint64_t>(b.value(), g.blocks_per_bank); if (!c) return Result<std::uint64_t>::failure(c.error());
  return checked_mul<std::uint64_t>(c.value(), g.pages_per_block);
}

Result<std::uint64_t> physical_capacity_bytes(const Geometry& g) {
  if (g.bytes_per_page == 0) return Result<std::uint64_t>::failure({ErrorCode::kInvalidArgument, "bytes_per_page must be set"});
  auto pages = physical_page_count(g); if (!pages) return pages;
  return checked_mul(pages.value(), static_cast<std::uint64_t>(g.bytes_per_page));
}

Result<void> validate(const Config& c) {
  if (c.schema_version != kConfigSchemaVersion)
    return Result<void>::failure({ErrorCode::kUnsupported, "unsupported configuration schema"});
  return c.ownership.validate(c.geometry);
}

Geometry from_legacy(const LegacyGeometry& l) {
  return Geometry{l.channels, l.core_dies_per_channel, l.dies_per_core,
                  l.banks_per_die, l.blocks_per_bank, l.pages_per_block,
                  l.bytes_per_page};
}

}  // namespace openhbf::common
