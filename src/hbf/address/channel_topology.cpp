#include "openhbx/hbf/address/channel_topology.h"

#include <algorithm>
#include <set>

namespace openhbx::hbf::address {

std::optional<ChannelOwnershipProfile> ChannelOwnershipProfile::synthetic_modulo(
    const HbfGeometry& geometry, std::string* error) {
  if (geometry.banks_per_die() % geometry.channels() != 0) {
    if (error != nullptr) *error = "synthetic modulo requires banks_per_die divisible by channels";
    return std::nullopt;
  }
  ChannelOwnershipProfile profile;
  profile.source = OwnershipSource::SyntheticFixture;
  profile.banks_by_channel.resize(geometry.channels());
  for (std::uint64_t core = 0; core < geometry.core_dies(); ++core) {
    for (std::uint64_t die = 0; die < geometry.dies_per_core(); ++die) {
      for (std::uint64_t bank = 0; bank < geometry.banks_per_die(); ++bank) {
        profile.banks_by_channel.at(bank % geometry.channels())
            .push_back({CoreDieIndex(core), DieIndex(die), BankIndex(bank)});
      }
    }
  }
  return profile;
}

std::optional<ChannelTopology> ChannelTopology::create(
    const HbfGeometry& geometry, ChannelOwnershipProfile profile, std::string* error) {
  if (profile.banks_by_channel.size() != geometry.channels()) {
    if (error != nullptr) *error = "ownership profile channel count mismatch";
    return std::nullopt;
  }
  std::set<PhysicalBank> all;
  for (auto& banks : profile.banks_by_channel) {
    if (banks.size() != geometry.owned_banks_per_channel()) {
      if (error != nullptr) *error = "each channel must own exactly R1*R2 banks";
      return std::nullopt;
    }
    for (const auto& bank : banks) {
      if (bank.core_die.value() >= geometry.core_dies() ||
          bank.die.value() >= geometry.dies_per_core() ||
          bank.bank.value() >= geometry.banks_per_die() || !all.insert(bank).second) {
        if (error != nullptr) *error = "ownership bank is out of range or duplicated";
        return std::nullopt;
      }
    }
  }
  return ChannelTopology(profile.source, std::move(profile.banks_by_channel));
}

std::optional<ChannelId> ChannelTopology::owner_of(const PhysicalBank& bank) const noexcept {
  for (std::size_t channel = 0; channel < banks_by_channel_.size(); ++channel) {
    const auto& banks = banks_by_channel_[channel];
    if (std::find(banks.begin(), banks.end(), bank) != banks.end()) {
      return ChannelId(channel);
    }
  }
  return std::nullopt;
}

bool ChannelTopology::can_access(ChannelId channel, const PhysicalBank& bank) const noexcept {
  const auto owner = owner_of(bank);
  return owner && *owner == channel;
}

std::optional<PhysicalBank> ChannelTopology::physical_bank(
    ChannelId channel, OwnedBankIndex index) const noexcept {
  if (channel.value() >= banks_by_channel_.size()) return std::nullopt;
  const auto& banks = banks_by_channel_[channel.value()];
  if (index.value() >= banks.size()) return std::nullopt;
  return banks[index.value()];
}

std::optional<OwnedBankIndex> ChannelTopology::owned_index(
    ChannelId channel, const PhysicalBank& bank) const noexcept {
  if (channel.value() >= banks_by_channel_.size()) return std::nullopt;
  const auto& banks = banks_by_channel_[channel.value()];
  const auto it = std::find(banks.begin(), banks.end(), bank);
  if (it == banks.end()) return std::nullopt;
  return OwnedBankIndex(static_cast<std::uint64_t>(std::distance(banks.begin(), it)));
}

}  // namespace openhbx::hbf::address
