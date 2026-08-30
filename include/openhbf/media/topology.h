#pragma once

#include <cstdint>

#include "openhbf/common/error.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

using PageKey = std::uint64_t;
using BlockKey = std::uint64_t;

struct ResolvedBlock {
  BlockKey key = 0;
  std::uint64_t first_page = 0;
  std::uint32_t page_count = 0;
};

struct ResolvedPage {
  PageKey key = 0;
  BlockKey block_key = 0;
  std::uint32_t page_in_block = 0;
};

struct TopologySnapshot {
  Geometry geometry;
  std::uint64_t block_count = 0;
  std::uint64_t page_count = 0;
};

// Immutable checked translation between the OCP address hierarchy and compact
// keys used by Media's sparse state owners.
class MediaTopology {
 public:
  static Result<MediaTopology> create(Geometry geometry);

  bool contains(const MediaAddress& address) const noexcept;
  Result<ResolvedPage> resolve(const MediaAddress& address) const;
  Result<ResolvedBlock> resolve_block(const MediaAddress& address) const;
  Result<MediaAddress> decode(PageKey key) const;

  const Geometry& geometry() const noexcept { return geometry_; }
  std::uint64_t block_count() const noexcept { return block_count_; }
  std::uint64_t page_count() const noexcept { return page_count_; }
  TopologySnapshot snapshot() const noexcept;

 private:
  MediaTopology(Geometry geometry, std::uint64_t blocks,
                std::uint64_t pages) noexcept;

  Geometry geometry_;
  std::uint64_t block_count_ = 0;
  std::uint64_t page_count_ = 0;
};

}  // namespace openhbf::media
