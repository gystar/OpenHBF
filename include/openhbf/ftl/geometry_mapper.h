#pragma once
#include "openhbf/ftl/types.h"
#include <cstdint>
#include <vector>
namespace openhbf::ftl {
struct GeometryProfile {
  // R4 is the number of 64-byte units in one 4 KiB DLU and is fixed at 64.
  std::uint64_t r1 = 1, r2 = 1, r3 = 1, r4 = 64, r5 = 1;
  std::uint16_t core_die_count = 1;
  std::uint16_t dies_per_core = 1;
  std::uint16_t banks_per_die = 1;
  std::uint32_t blocks_per_bank = 1;
  std::uint32_t pages_per_block = 1;
  std::uint16_t channels = 1;
  // Media cell mode is part of the shared geometry profile so FTL can select
  // the neutral page layout required by Media timing (not a NAND policy).
  media::CellMode cell_mode = media::CellMode::Slc;
};
struct MapResult {
  std::uint64_t l1 = 0;
  std::uint64_t b1 = 0;
  std::uint64_t p1 = 0;
  std::uint64_t bank_number = 0;
  std::uint64_t l2 = 0;
  media::MediaAddress address{};
};
struct ReplayRange {
  MapResult base{};
  std::vector<Dlu> dlu{};
};
class GeometryMapper {
 public:
  explicit GeometryMapper(GeometryProfile p): profile_(p) {}
  Result<MapResult> map(ChannelId, Dlu) const;
  Result<MapResult> map(ChannelId, Unit64) const;
  Result<MapResult> map(ChannelId, ByteAddress) const;
  const GeometryProfile& profile() const noexcept{return profile_;}
 private:
  Result<MapResult> map_units(ChannelId, std::uint64_t) const;
  GeometryProfile profile_;
};
Result<ReplayRange> calculate_replay_range(const GeometryMapper&, ChannelId, Dlu);
}
