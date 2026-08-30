#include "openhbf/media/eat.h"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

#include "openhbf/common/checked_math.h"

namespace openhbf::media {
namespace {

constexpr std::uint64_t kKindShift = 60;
constexpr std::uint64_t kChannelShift = 52;
constexpr std::uint64_t kCoreDieShift = 40;
constexpr std::uint64_t kDieShift = 28;
constexpr std::uint64_t kBankShift = 16;
constexpr std::uint64_t kChannelMax = 0xff;
constexpr std::uint64_t kIndexMax = 0xfff;

Result<ResourceId> make_resource(ResourceKind kind, const MediaAddress& address,
                                 bool include_channel, bool include_die,
                                 bool include_bank) {
  if (address.channel.value() > kChannelMax ||
      address.core_die > kIndexMax ||
      (include_die && address.die > kIndexMax) ||
      (include_bank && address.bank > kIndexMax)) {
    return Result<ResourceId>::failure(
        {ErrorCode::kOutOfRange, "media resource coordinate exceeds encoding"});
  }
  std::uint64_t value = (static_cast<std::uint64_t>(kind) + 1) << kKindShift;
  if (include_channel) {
    value |= static_cast<std::uint64_t>(address.channel.value()) << kChannelShift;
  }
  value |= static_cast<std::uint64_t>(address.core_die) << kCoreDieShift;
  if (include_die) {
    value |= static_cast<std::uint64_t>(address.die) << kDieShift;
  }
  if (include_bank) {
    value |= static_cast<std::uint64_t>(address.bank) << kBankShift;
  }
  return Result<ResourceId>::success(ResourceId(value));
}

}  // namespace

Result<ResourceId> bank_array_resource(const MediaAddress& address) {
  return make_resource(ResourceKind::BankArray, address, false, true, true);
}

Result<ResourceId> channel_media_path_resource(const MediaAddress& address) {
  return make_resource(ResourceKind::ChannelMediaPath, address, true, false,
                       false);
}

Result<ResourceId> vendor_resource(std::uint64_t vendor_id) {
  constexpr std::uint64_t kPayloadMask = (std::uint64_t{1} << kKindShift) - 1;
  if (vendor_id == 0 || vendor_id > kPayloadMask) {
    return Result<ResourceId>::failure(
        {ErrorCode::kOutOfRange, "vendor resource id is outside valid range"});
  }
  const auto kind = static_cast<std::uint64_t>(ResourceKind::VendorResource) + 1;
  return Result<ResourceId>::success(
      ResourceId((kind << kKindShift) | vendor_id));
}

EatTable::EatTable(std::vector<ResourceId> resources) {
  for (ResourceId resource : resources) {
    entries_.emplace(resource, State{});
  }
}

Result<Reservation> EatTable::preview(
    const std::vector<ResourceId>& resources, Cycle now,
    Duration duration) const {
  if (duration.value() == 0 || resources.empty()) {
    return Result<Reservation>::failure(
        {ErrorCode::kInvalidArgument,
         "EAT reservation requires resources and non-zero duration"});
  }

  Cycle start = now;
  std::unordered_set<ResourceId> unique;
  for (ResourceId resource : resources) {
    if (!unique.insert(resource).second) {
      return Result<Reservation>::failure(
          {ErrorCode::kInvalidArgument, "duplicate EAT resource"});
    }
    const auto it = entries_.find(resource);
    if (it == entries_.end()) {
      return Result<Reservation>::failure(
          {ErrorCode::kOutOfRange, "unknown EAT resource"});
    }
    if (start < it->second.next_available) {
      start = it->second.next_available;
    }
  }
  auto end = checked_add(start, duration);
  if (!end) {
    return Result<Reservation>::failure(end.error());
  }
  return Result<Reservation>::success(
      Reservation{start, end.value(), resources});
}

Result<Reservation> EatTable::reserve_atomic(
    const std::vector<ResourceId>& resources, Cycle now, Duration duration) {
  return reserve_atomic(MediaToken{}, resources, now, duration);
}

Result<Reservation> EatTable::reserve_atomic(
    MediaToken owner, const std::vector<ResourceId>& resources, Cycle now,
    Duration duration) {
  auto result = preview(resources, now, duration);
  if (!result) {
    return result;
  }
  // preview validated every resource and the end cycle. No mutation occurs
  // before that validation succeeds, so this group update is atomic.
  for (ResourceId resource : resources) {
    State& state = entries_.at(resource);
    state.next_available = result.value().end;
    state.reservations.push_back(
        {owner, result.value().start, result.value().end});
    ++state.version;
  }
  return result;
}

std::size_t EatTable::cancel(MediaToken owner, Cycle now) noexcept {
  std::size_t removed = 0;
  for (auto& entry : entries_) {
    auto& state = entry.second;
    const auto old_size = state.reservations.size();
    state.reservations.erase(
        std::remove_if(state.reservations.begin(), state.reservations.end(),
                       [&](const State::Interval& interval) {
                         return interval.owner == owner;
                       }),
        state.reservations.end());
    removed += old_size - state.reservations.size();
    if (old_size != state.reservations.size()) {
      state.next_available = now;
      for (const auto& interval : state.reservations) {
        if (state.next_available < interval.end) state.next_available = interval.end;
      }
      ++state.version;
    }
  }
  return removed;
}

std::vector<EatEntry> EatTable::snapshot() const {
  std::vector<EatEntry> result;
  result.reserve(entries_.size());
  for (const auto& entry : entries_) {
    result.push_back({entry.first, entry.second.next_available,
                      entry.second.version});
  }
  std::sort(result.begin(), result.end(),
            [](const EatEntry& lhs, const EatEntry& rhs) {
              return lhs.resource < rhs.resource;
            });
  return result;
}

}  // namespace openhbf::media
