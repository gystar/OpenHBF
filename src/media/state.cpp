#include "openhbf/media/state.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace openhbf::media {

PageState MediaStateStore::page_state(PageKey key) const noexcept {
  const auto it = pages_.find(key);
  return it == pages_.end() ? PageState::Erased : it->second;
}

BlockState MediaStateStore::block_state(BlockKey key) const noexcept {
  const auto it = blocks_.find(key);
  return it == blocks_.end() ? BlockState{} : it->second;
}

Result<std::uint64_t> MediaStateStore::pec(const MediaAddress& address) const {
  auto block = topology_.resolve_block(address);
  if (!block) return Result<std::uint64_t>::failure(block.error());
  return Result<std::uint64_t>::success(
      block_state(block.value().key).program_erase_count);
}

std::vector<BlockMetadata> MediaStateStore::export_blocks() const {
  std::vector<BlockMetadata> result;
  result.reserve(blocks_.size());
  for (const auto& entry : blocks_) result.push_back({entry.first, entry.second});
  std::sort(result.begin(), result.end(),
            [](const BlockMetadata& lhs, const BlockMetadata& rhs) {
              return lhs.key < rhs.key;
            });
  return result;
}

Result<void> MediaStateStore::import_blocks(
    const std::vector<BlockMetadata>& blocks) {
  std::unordered_map<BlockKey, BlockState> staged;
  for (const auto& entry : blocks) {
    if (entry.key >= topology_.block_count() ||
        entry.state.expected_page > topology_.geometry().pages_per_block) {
      return Result<void>::failure(
          {ErrorCode::kOutOfRange, "block metadata is outside topology"});
    }
    if (!staged.emplace(entry.key, entry.state).second) {
      return Result<void>::failure(
          {ErrorCode::kIntegrity, "duplicate block metadata key"});
    }
  }
  if (!reservations_.empty()) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "cannot restore block metadata while commands are reserved"});
  }
  blocks_ = std::move(staged);
  return Result<void>::success();
}

MediaStatus MediaStateStore::check(const MediaCommand& command) const {
  const auto resolved = topology_.resolve(command.address);
  if (!resolved) return MediaStatus::InvalidAddress;
  const auto& page = resolved.value();
  if (erase_owner_.find(page.block_key) != erase_owner_.end()) {
    return MediaStatus::Busy;
  }
  if (command.op == MediaOp::Erase) {
    for (const auto& reservation : reservations_) {
      if (reservation.second.page.block_key == page.block_key) {
        return MediaStatus::Busy;
      }
    }
  } else if (page_owner_.find(page.key) != page_owner_.end()) {
    return MediaStatus::Busy;
  }
  if (command.op == MediaOp::Read && page_state(page.key) != PageState::Valid) {
    return MediaStatus::ErasedPage;
  }
  if (command.op == MediaOp::Program) {
    const auto block = block_state(page.block_key);
    if (page.page_in_block != block.expected_page ||
        page_state(page.key) != PageState::Erased) {
      return MediaStatus::ProgramOrderViolation;
    }
  }
  return MediaStatus::Success;
}

Result<ReservationHandle> MediaStateStore::reserve(
    MediaToken token, const MediaCommand& command) {
  if (!is_valid_token(token)) {
    return Result<ReservationHandle>::failure(
        {ErrorCode::kInvalidArgument, "reservation requires a nonzero token"});
  }
  const auto legality = check(command);
  if (legality != MediaStatus::Success) {
    return Result<ReservationHandle>::failure(
        {legality == MediaStatus::Busy ? ErrorCode::kIntegrity
                                      : ErrorCode::kInvalidArgument,
         "media state rejected reservation"});
  }
  const auto page = topology_.resolve(command.address).value();
  const ReservationHandle handle(next_handle_++);
  TransientState transient = TransientState::ReadReserved;
  if (command.op == MediaOp::Program) transient = TransientState::ProgramReserved;
  if (command.op == MediaOp::Erase) transient = TransientState::EraseReserved;
  reservations_.emplace(handle.value(),
                        Reservation{token, command.generation, command.op, page,
                                    transient});
  if (command.op == MediaOp::Erase) {
    erase_owner_.emplace(page.block_key, handle.value());
  } else {
    page_owner_.emplace(page.key, handle.value());
  }
  return Result<ReservationHandle>::success(handle);
}

Result<void> MediaStateStore::commit_terminal(ReservationHandle handle,
                                               Generation generation,
                                               const StateDelta& delta,
                                               Cycle cycle) {
  const auto it = reservations_.find(handle.value());
  if (it == reservations_.end()) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "unknown or already completed reservation"});
  }
  if (it->second.generation != generation) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "reservation generation mismatch"});
  }
  const Reservation reservation = it->second;

  // All persistent mutations happen here, after terminal success is known.
  if (delta.success && reservation.op == MediaOp::Program) {
    pages_[reservation.page.key] = PageState::Valid;
    auto& block = blocks_[reservation.page.block_key];
    ++block.expected_page;
    block.last_program_cycle = cycle;
  } else if (delta.success && reservation.op == MediaOp::Erase) {
    const auto first = reservation.page.block_key *
                       topology_.geometry().pages_per_block;
    const auto end = first + topology_.geometry().pages_per_block;
    for (PageKey key = first; key < end; ++key) pages_.erase(key);
    auto& block = blocks_[reservation.page.block_key];
    block.expected_page = 0;
    ++block.program_erase_count;
    ++block.epoch;
    block.read_count = 0;
  } else if (!delta.success && delta.page_became_unknown &&
             reservation.op == MediaOp::Program) {
    pages_[reservation.page.key] = PageState::Unknown;
  }

  if (reservation.op == MediaOp::Erase) {
    erase_owner_.erase(reservation.page.block_key);
  } else {
    page_owner_.erase(reservation.page.key);
  }
  reservations_.erase(it);
  return Result<void>::success();
}

Result<void> MediaStateStore::abort(ReservationHandle handle,
                                    Generation generation) {
  const auto it = reservations_.find(handle.value());
  if (it == reservations_.end()) {
    return Result<void>::failure({ErrorCode::kIntegrity,
                                  "unknown or already completed reservation"});
  }
  if (it->second.generation != generation) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "reservation generation mismatch"});
  }
  if (it->second.op == MediaOp::Erase) {
    erase_owner_.erase(it->second.page.block_key);
  } else {
    page_owner_.erase(it->second.page.key);
  }
  reservations_.erase(it);
  return Result<void>::success();
}

std::size_t MediaStateStore::abort_generation(Generation generation) noexcept {
  std::size_t count = 0;
  for (auto it = reservations_.begin(); it != reservations_.end();) {
    if (it->second.generation == generation) {
      if (it->second.op == MediaOp::Erase) {
        erase_owner_.erase(it->second.page.block_key);
      } else {
        page_owner_.erase(it->second.page.key);
      }
      it = reservations_.erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

std::uint64_t MediaStateStore::increment_read_count(BlockKey key) {
  auto& count = blocks_[key].read_count;
  if (count != std::numeric_limits<std::uint64_t>::max()) ++count;
  return count;
}

StateSnapshot MediaStateStore::snapshot() const noexcept {
  return {pages_.size(), blocks_.size(), reservations_.size()};
}

}  // namespace openhbf::media
