#include "openhbf/media/topology.h"

#include <limits>
#include <string>

namespace openhbf::media {
namespace {

Result<std::uint64_t> checked_mul(std::uint64_t lhs, std::uint64_t rhs,
                                  const char* field) {
  if (rhs != 0 && lhs > std::numeric_limits<std::uint64_t>::max() / rhs) {
    return Result<std::uint64_t>::failure(
        {ErrorCode::kOverflow, std::string("topology overflow at ") + field});
  }
  return Result<std::uint64_t>::success(lhs * rhs);
}

}  // namespace

MediaTopology::MediaTopology(Geometry geometry, std::uint64_t blocks,
                             std::uint64_t pages) noexcept
    : geometry_(geometry), block_count_(blocks), page_count_(pages) {}

Result<MediaTopology> MediaTopology::create(Geometry geometry) {
  const auto valid = validate(geometry);
  if (!valid) {
    return Result<MediaTopology>::failure(valid.error());
  }
  std::uint64_t blocks = 1;
  for (const auto factor : {static_cast<std::uint64_t>(geometry.core_dies_per_channel),
                            static_cast<std::uint64_t>(geometry.dies_per_core),
                            static_cast<std::uint64_t>(geometry.banks_per_die),
                            static_cast<std::uint64_t>(geometry.blocks_per_bank)}) {
    auto next = checked_mul(blocks, factor, "block_count");
    if (!next) return Result<MediaTopology>::failure(next.error());
    blocks = next.value();
  }
  auto pages = checked_mul(blocks, geometry.pages_per_block, "page_count");
  if (!pages) return Result<MediaTopology>::failure(pages.error());
  return Result<MediaTopology>::success(
      MediaTopology(geometry, blocks, pages.value()));
}

bool MediaTopology::contains(const MediaAddress& a) const noexcept {
  return a.channel.value() < geometry_.channels &&
         a.core_die < geometry_.core_dies_per_channel &&
         a.die < geometry_.dies_per_core && a.bank < geometry_.banks_per_die &&
         a.block < geometry_.blocks_per_bank && a.page < geometry_.pages_per_block &&
         owner_channel(geometry_, a) == a.channel;
}

Result<ResolvedBlock> MediaTopology::resolve_block(const MediaAddress& a) const {
  if (!contains(a)) {
    return Result<ResolvedBlock>::failure(
        {ErrorCode::kOutOfRange, "media address is outside topology"});
  }
  std::uint64_t key = a.core_die;
  key = key * geometry_.dies_per_core + a.die;
  key = key * geometry_.banks_per_die + a.bank;
  key = key * geometry_.blocks_per_bank + a.block;
  return Result<ResolvedBlock>::success(
      {key, key * geometry_.pages_per_block, geometry_.pages_per_block});
}

Result<ResolvedPage> MediaTopology::resolve(const MediaAddress& a) const {
  auto block = resolve_block(a);
  if (!block) return Result<ResolvedPage>::failure(block.error());
  return Result<ResolvedPage>::success(
      {block.value().first_page + a.page, block.value().key, a.page});
}

Result<MediaAddress> MediaTopology::decode(PageKey key) const {
  if (key >= page_count_) {
    return Result<MediaAddress>::failure(
        {ErrorCode::kOutOfRange, "page key is outside topology"});
  }
  MediaAddress a;
  a.page = static_cast<std::uint32_t>(key % geometry_.pages_per_block);
  key /= geometry_.pages_per_block;
  a.block = static_cast<std::uint32_t>(key % geometry_.blocks_per_bank);
  key /= geometry_.blocks_per_bank;
  a.bank = static_cast<std::uint16_t>(key % geometry_.banks_per_die);
  key /= geometry_.banks_per_die;
  a.die = static_cast<std::uint16_t>(key % geometry_.dies_per_core);
  key /= geometry_.dies_per_core;
  a.core_die = static_cast<std::uint16_t>(key % geometry_.core_dies_per_channel);
  a.channel = owner_channel(geometry_, a);
  return Result<MediaAddress>::success(a);
}

TopologySnapshot MediaTopology::snapshot() const noexcept {
  return {geometry_, block_count_, page_count_};
}

}  // namespace openhbf::media
