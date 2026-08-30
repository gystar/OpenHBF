#include "openhbf/media/bbt.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "openhbf/common/checked_math.h"

namespace openhbf::media {
namespace {

MediaAddress block_address(MediaAddress address) noexcept {
  address.page = 0;
  return address;
}

std::uint64_t mix(std::uint64_t value) noexcept {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

}  // namespace

Result<BadBlockTable> BadBlockTable::create(Geometry geometry) {
  auto valid = validate(geometry);
  if (!valid) return Result<BadBlockTable>::failure(valid.error());
  return Result<BadBlockTable>::success(BadBlockTable(geometry));
}

bool BadBlockTable::Less::operator()(const MediaAddress& lhs,
                                     const MediaAddress& rhs) const noexcept {
  if (lhs.core_die != rhs.core_die) return lhs.core_die < rhs.core_die;
  if (lhs.die != rhs.die) return lhs.die < rhs.die;
  if (lhs.bank != rhs.bank) return lhs.bank < rhs.bank;
  return lhs.block < rhs.block;
}

bool BadBlockTable::contains_block(const MediaAddress& a) const noexcept {
  return a.channel.value() < geometry_.channels &&
         a.core_die < geometry_.core_dies_per_channel &&
         a.die < geometry_.dies_per_core && a.bank < geometry_.banks_per_die &&
         a.block < geometry_.blocks_per_bank &&
         owner_channel(geometry_, a) == a.channel;
}

Result<bool> BadBlockTable::is_bad(const MediaAddress& address) const {
  if (!contains_block(address)) {
    return Result<bool>::failure(
        {ErrorCode::kOutOfRange, "bad-block query is outside geometry"});
  }
  return Result<bool>::success(
      entries_.find(block_address(address)) != entries_.end());
}

Result<MarkResult> BadBlockTable::mark_bad(
    const MediaAddress& address, BadReason reason, Cycle cycle,
    std::uint64_t pe_cycles) {
  if (!contains_block(address)) {
    return Result<MarkResult>::failure(
        {ErrorCode::kOutOfRange, "bad-block address is outside geometry"});
  }
  const MediaAddress key = block_address(address);
  if (entries_.find(key) != entries_.end()) {
    return Result<MarkResult>::success(MarkResult::AlreadyBad);
  }
  entries_.emplace(key, BadBlockEntry{key, reason, cycle, pe_cycles});
  ++version_;
  return Result<MarkResult>::success(MarkResult::Marked);
}

Result<void> BadBlockTable::replace(BbtSnapshot snapshot) {
  std::map<MediaAddress, BadBlockEntry, Less> staged;
  for (auto entry : snapshot.entries) {
    entry.block_address.page = 0;
    if (!contains_block(entry.block_address) ||
        !staged.emplace(entry.block_address, entry).second) {
      return Result<void>::failure(
          {ErrorCode::kIntegrity, "invalid or duplicate BBT restore entry"});
    }
  }
  entries_ = std::move(staged);
  version_ = snapshot.version;
  return Result<void>::success();
}

BbtSnapshot BadBlockTable::snapshot() const {
  BbtSnapshot result;
  result.version = version_;
  for (const auto& entry : entries_) result.entries.push_back(entry.second);
  return result;
}

RetirementMap::RetirementMap(Geometry geometry,
                             std::uint64_t blocks_per_channel,
                             std::uint64_t)
    : geometry_(geometry), blocks_per_channel_(blocks_per_channel) {}

Result<RetirementMap> RetirementMap::create(Geometry geometry) {
  auto valid = validate(geometry);
  if (!valid) return Result<RetirementMap>::failure(valid.error());
  std::uint64_t per_channel = geometry.core_dies_per_channel;
  for (const std::uint64_t factor : {
           std::uint64_t{geometry.dies_per_core},
           static_cast<std::uint64_t>(geometry.banks_per_die) /
               geometry.channels,
           std::uint64_t{geometry.blocks_per_bank}}) {
    auto product = checked_mul(per_channel, factor);
    if (!product) return Result<RetirementMap>::failure(product.error());
    per_channel = product.value();
  }
  auto total = checked_mul(per_channel, std::uint64_t{geometry.channels});
  if (!total) return Result<RetirementMap>::failure(total.error());
  return Result<RetirementMap>::success(
      RetirementMap(geometry, per_channel, total.value()));
}

bool RetirementMap::contains(const MediaAddress& a) const noexcept {
  return a.channel.value() < geometry_.channels &&
         a.core_die < geometry_.core_dies_per_channel &&
         a.die < geometry_.dies_per_core && a.bank < geometry_.banks_per_die &&
         a.block < geometry_.blocks_per_bank &&
         owner_channel(geometry_, a) == a.channel;
}

std::uint64_t RetirementMap::channel_block_index(
    const MediaAddress& a) const noexcept {
  std::uint64_t index = a.core_die;
  index = index * geometry_.dies_per_core + a.die;
  index = index * (geometry_.banks_per_die / geometry_.channels) +
          (a.bank / geometry_.channels);
  return index * geometry_.blocks_per_bank + a.block;
}

std::uint64_t RetirementMap::global_block_index(
    const MediaAddress& a) const noexcept {
  return a.channel.value() * blocks_per_channel_ + channel_block_index(a);
}

MediaAddress RetirementMap::address_from_global(std::uint64_t index) const noexcept {
  MediaAddress address;
  address.channel = ChannelId(static_cast<std::uint16_t>(index / blocks_per_channel_));
  index %= blocks_per_channel_;
  address.block = static_cast<std::uint32_t>(index % geometry_.blocks_per_bank);
  index /= geometry_.blocks_per_bank;
  const auto banks_per_channel = geometry_.banks_per_die / geometry_.channels;
  const auto bank_slot = static_cast<std::uint16_t>(index % banks_per_channel);
  address.bank = static_cast<std::uint16_t>(
      bank_slot * geometry_.channels + address.channel.value());
  index /= banks_per_channel;
  address.die = static_cast<std::uint16_t>(index % geometry_.dies_per_core);
  index /= geometry_.dies_per_core;
  address.core_die = static_cast<std::uint16_t>(index);
  return address;
}

Result<std::vector<std::uint64_t>> RetirementMap::expand(
    const MediaScope& scope) const {
  if (!contains(scope.address)) {
    return Result<std::vector<std::uint64_t>>::failure(
        {ErrorCode::kOutOfRange, "retirement scope is outside geometry"});
  }
  const auto append_range = [](std::vector<std::uint64_t>& out,
                               std::uint64_t first, std::uint64_t count) {
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) out.push_back(first + i);
  };
  std::vector<std::uint64_t> result;
  const std::uint64_t block = global_block_index(scope.address);
  switch (scope.kind) {
    case ScopeKind::Block:
      result.push_back(block);
      break;
    case ScopeKind::Bank:
      append_range(result, block - scope.address.block,
                   geometry_.blocks_per_bank);
      break;
    case ScopeKind::Die: {
      const std::uint64_t count =
          (static_cast<std::uint64_t>(geometry_.banks_per_die) /
           geometry_.channels) *
          geometry_.blocks_per_bank;
      append_range(result, block - channel_block_index(scope.address) % count,
                   count);
      break;
    }
    case ScopeKind::CoreDie: {
      const std::uint64_t count = std::uint64_t{geometry_.dies_per_core} *
          (geometry_.banks_per_die / geometry_.channels) *
          geometry_.blocks_per_bank;
      append_range(result, block - channel_block_index(scope.address) % count,
                   count);
      break;
    }
    case ScopeKind::Channel:
      append_range(result,
                   std::uint64_t{scope.address.channel.value()} * blocks_per_channel_,
                   blocks_per_channel_);
      break;
  }
  return Result<std::vector<std::uint64_t>>::success(std::move(result));
}

Result<void> RetirementMap::retire(const MediaScope& scope,
                                   RetireReason reason, Cycle cycle) {
  auto indices = expand(scope);
  if (!indices) return Result<void>::failure(indices.error());
  bool changed = false;
  for (const auto index : indices.value()) {
    if (entries_.find(index) == entries_.end()) {
      entries_.emplace(index,
                       RetirementEntry{address_from_global(index), reason, cycle});
      changed = true;
    }
  }
  if (changed) ++version_;
  return Result<void>::success();
}

Result<bool> RetirementMap::is_retired(const MediaAddress& address) const {
  if (!contains(address)) {
    return Result<bool>::failure(
        {ErrorCode::kOutOfRange, "retirement query is outside geometry"});
  }
  return Result<bool>::success(
      entries_.find(global_block_index(address)) != entries_.end());
}

Result<BitmapPage> RetirementMap::bitmap_page(
    ChannelId channel, std::uint64_t byte_offset) const {
  const std::uint64_t bitmap_bytes = (blocks_per_channel_ + 7U) / 8U;
  if (channel.value() >= geometry_.channels || byte_offset >= bitmap_bytes) {
    return Result<BitmapPage>::failure(
        {ErrorCode::kOutOfRange, "retirement bitmap page is outside channel range"});
  }
  BitmapPage page;
  page.channel = channel;
  page.byte_offset = byte_offset;
  page.version = version_;
  for (std::size_t out = 0; out < BitmapPage::kBytes; ++out) {
    const std::uint64_t source_byte = byte_offset + out;
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
      const std::uint64_t block = source_byte * 8U + bit;
      if (block >= blocks_per_channel_) break;
      const std::uint64_t global = channel.value() * blocks_per_channel_ + block;
      if (entries_.find(global) != entries_.end())
        page.bytes[out] |= static_cast<std::uint8_t>(1U << bit);
    }
  }
  return Result<BitmapPage>::success(page);
}

RetirementSnapshot RetirementMap::snapshot() const {
  RetirementSnapshot result;
  result.version = version_;
  for (const auto& entry : entries_) result.entries.push_back(entry.second);
  return result;
}

Result<void> RetirementMap::replace(RetirementSnapshot snapshot) {
  std::map<std::uint64_t, RetirementEntry> staged;
  for (auto entry : snapshot.entries) {
    entry.block_address.page = 0;
    if (!contains(entry.block_address) ||
        !staged.emplace(global_block_index(entry.block_address), entry).second) {
      return Result<void>::failure(
          {ErrorCode::kIntegrity, "invalid or duplicate retirement restore entry"});
    }
  }
  entries_ = std::move(staged);
  version_ = snapshot.version;
  return Result<void>::success();
}

Result<void> initialize_factory_bad_blocks(
    BadBlockTable& table, const Geometry& geometry,
    const ReliabilityConfig& config) {
  for (const auto& address : config.factory_bad_blocks) {
    auto marked = table.mark_bad(address, BadReason::Factory, Cycle{}, 0);
    if (!marked) return Result<void>::failure(marked.error());
  }
  if (config.factory_bad_rate == 0.0) return Result<void>::success();
  const long double threshold = config.factory_bad_rate *
      static_cast<long double>(std::numeric_limits<std::uint64_t>::max());
  std::uint64_t ordinal = 0;
  for (std::uint16_t core = 0; core < geometry.core_dies_per_channel; ++core)
      for (std::uint16_t die = 0; die < geometry.dies_per_core; ++die)
        for (std::uint16_t bank = 0; bank < geometry.banks_per_die; ++bank)
          for (std::uint32_t block = 0; block < geometry.blocks_per_bank;
               ++block, ++ordinal) {
            MediaAddress address{ChannelId(0), core, die, bank, block, 0};
            address.channel = owner_channel(geometry, address);
            if (static_cast<long double>(mix(config.seed ^ ordinal)) < threshold) {
              auto marked = table.mark_bad(address, BadReason::Factory,
                                           Cycle{}, 0);
              if (!marked) return Result<void>::failure(marked.error());
            }
          }
  return Result<void>::success();
}

}  // namespace openhbf::media
