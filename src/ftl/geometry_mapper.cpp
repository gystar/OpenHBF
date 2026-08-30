#include "openhbf/ftl/geometry_mapper.h"
#include <limits>
#include <utility>
#include "openhbf/common/checked_math.h"
namespace openhbf::ftl {
Result<MapResult> GeometryMapper::map(ChannelId channel, Dlu dlu) const {
  return map_units(channel, dlu.value());
}
Result<MapResult> GeometryMapper::map(ChannelId channel, Unit64 units) const {
  if (profile_.r4 == 0 || units.value() % profile_.r4 != 0)
    return Result<MapResult>::failure({ErrorCode::kInvalidArgument,"64-byte unit is not DLU aligned"});
  return map_units(channel, units.value() / profile_.r4);
}
Result<MapResult> GeometryMapper::map(ChannelId channel, ByteAddress bytes) const {
  if (bytes.value() % kDluBytes != 0) return Result<MapResult>::failure({ErrorCode::kInvalidArgument,"byte address is not 64-byte aligned"});
  return map(channel, Unit64(bytes.value() / kDluBytes));
}
Result<MapResult> GeometryMapper::map_units(ChannelId channel, std::uint64_t l1) const {
  if (channel.value() >= profile_.channels || profile_.r1 == 0 ||
      profile_.r2 == 0 || profile_.r3 == 0 || profile_.r4 != 64 ||
      profile_.r5 == 0 || profile_.core_die_count == 0 ||
      profile_.dies_per_core == 0 || profile_.banks_per_die == 0 ||
      profile_.blocks_per_bank == 0 || profile_.pages_per_block == 0) {
    return Result<MapResult>::failure(
        {ErrorCode::kInvalidArgument, "invalid FTL geometry profile"});
  }
  auto r12 = checked_mul(profile_.r1, profile_.r2);
  if (!r12) return Result<MapResult>::failure(r12.error());
  auto span = checked_mul(r12.value(), profile_.r3);
  if (!span) return Result<MapResult>::failure(span.error());
  const auto block = l1 / span.value();
  const auto page = l1 % span.value();
  const auto bank_index = page % r12.value();
  const auto bank = bank_index % profile_.banks_per_die;
  const auto die = (bank_index / profile_.banks_per_die) % profile_.dies_per_core;
  const auto core = (bank_index / profile_.banks_per_die /
                     profile_.dies_per_core) % profile_.core_die_count;
  if (bank >= profile_.banks_per_die || die >= profile_.dies_per_core ||
      core >= profile_.core_die_count ||
      block >= profile_.blocks_per_bank || page >= profile_.pages_per_block ||
      block > std::numeric_limits<std::uint32_t>::max() ||
      page > std::numeric_limits<std::uint32_t>::max()) {
    return Result<MapResult>::failure(
        {ErrorCode::kOutOfRange, "mapped address exceeds Media geometry"});
  }
  MapResult result;
  result.l1 = l1;
  result.b1 = block;
  result.p1 = page;
  result.bank_number = bank_index;
  auto l2 = checked_mul(block, span.value());
  if (!l2) return Result<MapResult>::failure(l2.error());
  auto l2_with_bank = checked_add(l2.value(), bank_index);
  if (!l2_with_bank) return Result<MapResult>::failure(l2_with_bank.error());
  result.l2 = l2_with_bank.value();
  result.address = {channel, static_cast<std::uint16_t>(core),
                    static_cast<std::uint16_t>(die),
                    static_cast<std::uint16_t>(bank),
                    static_cast<std::uint32_t>(block),
                    static_cast<std::uint32_t>(page)};
  return Result<MapResult>::success(result);
}

Result<ReplayRange> calculate_replay_range(const GeometryMapper& mapper,
                                           ChannelId channel, Dlu start) {
  auto base = mapper.map(channel, start);
  if (!base) return Result<ReplayRange>::failure(base.error());
  const auto count = mapper.profile().r3;
  ReplayRange out;
  out.base = base.value();
  out.dlu.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t i = 0; i < count; ++i) {
    auto offset = checked_mul(i, mapper.profile().r5);
    if (!offset) return Result<ReplayRange>::failure(offset.error());
    auto address = checked_add(start.value(), offset.value());
    if (!address) return Result<ReplayRange>::failure(address.error());
    auto mapped = mapper.map(channel, Dlu(address.value()));
    if (!mapped) return Result<ReplayRange>::failure(mapped.error());
    out.dlu.emplace_back(address.value());
  }
  return Result<ReplayRange>::success(std::move(out));
}
}
