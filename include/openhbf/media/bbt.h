#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

enum class BadReason : std::uint8_t { Factory, ProgramFailure, EraseFailure, Wear };
enum class RetireReason : std::uint8_t { BlockBad, DieFailed, PathFailed, Administrative };
enum class ScopeKind : std::uint8_t { Block, Bank, Die, CoreDie, Channel };
enum class MarkResult : std::uint8_t { Marked, AlreadyBad };

struct MediaScope {
  ScopeKind kind = ScopeKind::Block;
  MediaAddress address;
};

struct BadBlockEntry {
  MediaAddress block_address;
  BadReason reason = BadReason::Factory;
  Cycle marked_cycle{};
  // PEC belongs to MediaStateStore. BBT retains this immutable diagnostic
  // snapshot from the instant the block became bad.
  std::uint64_t pe_cycles = 0;
};

struct BbtSnapshot {
  std::uint64_t version = 0;
  std::vector<BadBlockEntry> entries;
};

struct RetirementEntry {
  MediaAddress block_address;
  RetireReason reason = RetireReason::Administrative;
  Cycle retired_cycle{};
};

struct RetirementSnapshot {
  std::uint64_t version = 0;
  std::vector<RetirementEntry> entries;
};

struct BitmapPage {
  static constexpr std::size_t kBytes = 64;
  ChannelId channel{};
  std::uint64_t byte_offset = 0;
  std::uint64_t version = 0;
  std::array<std::uint8_t, kBytes> bytes{};
};

class BadBlockTable {
 public:
  static Result<BadBlockTable> create(Geometry geometry);

  Result<bool> is_bad(const MediaAddress& address) const;
  Result<MarkResult> mark_bad(const MediaAddress& address, BadReason reason,
                              Cycle cycle, std::uint64_t pe_cycles);
  Result<void> replace(BbtSnapshot snapshot);
  BbtSnapshot snapshot() const;

 private:
  struct Less {
    bool operator()(const MediaAddress&, const MediaAddress&) const noexcept;
  };
  explicit BadBlockTable(Geometry geometry) : geometry_(geometry) {}
  bool contains_block(const MediaAddress& address) const noexcept;

  Geometry geometry_;
  std::uint64_t version_ = 0;
  std::map<MediaAddress, BadBlockEntry, Less> entries_;
};

class RetirementMap {
 public:
  static Result<RetirementMap> create(Geometry geometry);

  Result<void> retire(const MediaScope& scope, RetireReason reason,
                      Cycle cycle = Cycle{});
  Result<bool> is_retired(const MediaAddress& address) const;
  Result<BitmapPage> bitmap_page(ChannelId channel,
                                 std::uint64_t byte_offset) const;
  Result<void> replace(RetirementSnapshot snapshot);
  RetirementSnapshot snapshot() const;
  std::uint64_t version() const noexcept { return version_; }
  std::uint64_t retired_blocks() const noexcept { return entries_.size(); }

 private:
  explicit RetirementMap(Geometry geometry, std::uint64_t blocks_per_channel,
                         std::uint64_t total_blocks);
  Result<std::vector<std::uint64_t>> expand(const MediaScope& scope) const;
  bool contains(const MediaAddress& address) const noexcept;
  MediaAddress address_from_global(std::uint64_t index) const noexcept;
  std::uint64_t channel_block_index(const MediaAddress& address) const noexcept;
  std::uint64_t global_block_index(const MediaAddress& address) const noexcept;

  Geometry geometry_;
  std::uint64_t blocks_per_channel_ = 0;
  std::uint64_t version_ = 0;
  std::map<std::uint64_t, RetirementEntry> entries_;
};

Result<void> initialize_factory_bad_blocks(BadBlockTable& table,
                                           const Geometry& geometry,
                                           const ReliabilityConfig& config);

}  // namespace openhbf::media
