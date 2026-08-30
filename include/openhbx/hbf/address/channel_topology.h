#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "openhbx/hbf/address/hbf_address_types.h"
#include "openhbx/hbf/address/hbf_geometry.h"

namespace openhbx::hbf::address {

enum class OwnershipSource { ProductProfile, SyntheticFixture };

struct ChannelOwnershipProfile {
  OwnershipSource source{OwnershipSource::ProductProfile};
  std::vector<std::vector<PhysicalBank>> banks_by_channel;

  static std::optional<ChannelOwnershipProfile> synthetic_modulo(
      const HbfGeometry& geometry, std::string* error = nullptr);
};

class ChannelTopology {
 public:
  static std::optional<ChannelTopology> create(
      const HbfGeometry& geometry, ChannelOwnershipProfile profile,
      std::string* error = nullptr);

  std::optional<ChannelId> owner_of(const PhysicalBank& bank) const noexcept;
  bool can_access(ChannelId channel, const PhysicalBank& bank) const noexcept;
  std::optional<PhysicalBank> physical_bank(ChannelId channel,
                                             OwnedBankIndex index) const noexcept;
  std::optional<OwnedBankIndex> owned_index(ChannelId channel,
                                            const PhysicalBank& bank) const noexcept;
  OwnershipSource source() const noexcept { return source_; }

 private:
  ChannelTopology(OwnershipSource source,
                  std::vector<std::vector<PhysicalBank>> banks_by_channel)
      : source_(source), banks_by_channel_(std::move(banks_by_channel)) {}
  OwnershipSource source_;
  std::vector<std::vector<PhysicalBank>> banks_by_channel_;
};

}  // namespace openhbx::hbf::address
