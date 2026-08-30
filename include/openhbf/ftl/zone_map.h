#pragma once

#include "openhbf/ftl/types.h"
#include <cstdint>
#include <vector>
#include <unordered_set>

namespace openhbf::ftl {

struct QuiesceProof { Generation generation{}; bool quiesced = false; };
struct ZoneSwapRequest { ChannelId channel{}; std::uint32_t first{}; std::uint32_t second{}; std::uint64_t zone_blocks{}; QuiesceProof proof{}; };

class ZoneMap {
 public:
  static Result<ZoneMap> create(ChannelId channel, std::uint32_t zones,
                                std::uint64_t zone_blocks);
  Result<std::uint32_t> translate(std::uint32_t logical) const;
  Result<void> validate_swap(const ZoneSwapRequest&) const;
  Result<void> swap(const ZoneSwapRequest&);
  Generation mapping_generation() const noexcept { return generation_; }
 private:
  ChannelId channel_{}; std::uint64_t zone_blocks_{}; Generation generation_{};
  std::vector<std::uint32_t> map_;
};

class RetiredCapacityMap {
 public:
  bool retire(const BlockKey& block) { return blocks_.insert(block).second; }
  bool contains(const BlockKey& block) const { return blocks_.count(block) != 0; }
  bool contains(const PhysicalDluAddress& address) const;
  std::vector<BlockKey> snapshot() const;
 private: std::unordered_set<BlockKey> blocks_;
};

} // namespace openhbf::ftl
