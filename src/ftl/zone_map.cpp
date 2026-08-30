#include "openhbf/ftl/zone_map.h"
#include <limits>
namespace openhbf::ftl {
namespace { Error err(ErrorCode c, const char* m){ return Error{c,m}; } }
Result<ZoneMap> ZoneMap::create(ChannelId channel, std::uint32_t zones, std::uint64_t zone_blocks) {
  if (zones == 0 || zone_blocks == 0) return Result<ZoneMap>::failure(err(ErrorCode::kInvalidArgument,"empty zone geometry"));
  ZoneMap z; z.channel_=channel; z.zone_blocks_=zone_blocks; z.map_.resize(zones);
  for (std::uint32_t i=0;i<zones;++i) z.map_[i]=i;
  return Result<ZoneMap>::success(std::move(z));
}
Result<std::uint32_t> ZoneMap::translate(std::uint32_t logical) const {
  if (logical >= map_.size()) return Result<std::uint32_t>::failure(err(ErrorCode::kOutOfRange,"zone index"));
  return Result<std::uint32_t>::success(map_[logical]);
}
Result<void> ZoneMap::validate_swap(const ZoneSwapRequest& r) const {
  if (r.channel != channel_ || !r.proof.quiesced || r.proof.generation != generation_)
    return Result<void>::failure(err(ErrorCode::kInvalidArgument,"invalid quiesce proof"));
  if (r.first >= map_.size() || r.second >= map_.size() || r.first == r.second || r.zone_blocks != zone_blocks_)
    return Result<void>::failure(err(ErrorCode::kInvalidArgument,"invalid zone swap"));
  return Result<void>::success();
}
Result<void> ZoneMap::swap(const ZoneSwapRequest& r) {
  auto ok=validate_swap(r); if(!ok) return ok;
  std::swap(map_[r.first],map_[r.second]);
  generation_=Generation(generation_.value()+1);
  return Result<void>::success();
}
bool RetiredCapacityMap::contains(const PhysicalDluAddress& a) const {
  return contains(BlockKey{a.owner_channel,a.core_die,a.die,a.bank,a.block});
}
std::vector<BlockKey> RetiredCapacityMap::snapshot() const { return {blocks_.begin(),blocks_.end()}; }
}
