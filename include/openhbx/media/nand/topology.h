#pragma once

#include <cstdint>
#include <optional>

#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/media/types.h"

namespace openhbx::media::nand {

class MediaTopology {
 public:
  explicit MediaTopology(config::HbfGeometry geometry);
  const config::HbfGeometry& geometry() const noexcept { return geometry_; }
  bool valid(const PhysicalAddress& address) const noexcept;
  std::optional<std::uint64_t> flatten_page(const PhysicalAddress& address) const noexcept;
  std::optional<PhysicalAddress> decode_page(std::uint64_t index) const noexcept;
  std::optional<std::uint64_t> flatten_block(const PhysicalAddress& address) const noexcept;
  std::uint64_t bank_id(const PhysicalAddress& address) const noexcept;
  std::uint64_t die_id(const PhysicalAddress& address) const noexcept;
 private:
  config::HbfGeometry geometry_;
};

}  // namespace openhbx::media::nand
