#pragma once

#include <cstdint>
#include <unordered_map>

#include "openhbf/common/error.h"
#include "openhbf/media/topology.h"

namespace openhbf::media {

enum class PageState : std::uint8_t { Erased, Valid, Unknown };
enum class TransientState : std::uint8_t {
  None,
  ReadReserved,
  ProgramReserved,
  EraseReserved,
};

struct ReservationHandleTag;
using ReservationHandle = StrongValue<ReservationHandleTag, std::uint64_t>;

struct BlockState {
  std::uint32_t expected_page = 0;
  std::uint64_t program_erase_count = 0;
  std::uint64_t epoch = 0;
  std::uint64_t read_count = 0;
  Cycle last_program_cycle{};
};

struct StateDelta {
  bool success = false;
  // A destructive array failure may leave the target page's contents unknown.
  bool page_became_unknown = false;
};

struct StateSnapshot {
  std::size_t allocated_pages = 0;
  std::size_t allocated_blocks = 0;
  std::size_t reservations = 0;
};

struct BlockMetadata {
  BlockKey key = 0;
  BlockState state;
};

class MediaStateStore {
 public:
  explicit MediaStateStore(const MediaTopology& topology) : topology_(topology) {}

  MediaStatus check(const MediaCommand& command) const;
  Result<ReservationHandle> reserve(MediaToken token,
                                    const MediaCommand& command);
  Result<void> commit_terminal(ReservationHandle handle,
                               Generation generation,
                               const StateDelta& delta, Cycle cycle);
  Result<void> abort(ReservationHandle handle, Generation generation);
  std::size_t abort_generation(Generation generation) noexcept;

  PageState page_state(PageKey key) const noexcept;
  BlockState block_state(BlockKey key) const noexcept;
  Result<std::uint64_t> pec(const MediaAddress& address) const;
  std::vector<BlockMetadata> export_blocks() const;
  Result<void> import_blocks(const std::vector<BlockMetadata>& blocks);
  std::uint64_t increment_read_count(BlockKey key);
  StateSnapshot snapshot() const noexcept;

 private:
  struct Reservation {
    MediaToken token;
    Generation generation;
    MediaOp op;
    ResolvedPage page;
    TransientState transient;
  };

  const MediaTopology& topology_;
  std::uint64_t next_handle_ = 1;
  std::unordered_map<PageKey, PageState> pages_;
  std::unordered_map<BlockKey, BlockState> blocks_;
  std::unordered_map<std::uint64_t, Reservation> reservations_;
  std::unordered_map<PageKey, std::uint64_t> page_owner_;
  std::unordered_map<BlockKey, std::uint64_t> erase_owner_;
};

}  // namespace openhbf::media
