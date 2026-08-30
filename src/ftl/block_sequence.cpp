#include "openhbf/ftl/block_sequence.h"

namespace openhbf::ftl {

Result<SequenceReservation> BlockSequenceTable::reserve(
    const BlockKey& block, std::uint32_t page, FtlToken token, Generation generation) {
  if (!is_valid_token(token)) {
    return Result<SequenceReservation>::failure(
        {ErrorCode::kInvalidArgument, "invalid FTL token"});
  }
  auto& state = states_[block];
  if (state.reservation) {
    return Result<SequenceReservation>::failure(
        {ErrorCode::kIntegrity, "block already has a workflow reservation"});
  }
  const SequenceReservation reservation{token, generation, block, page};
  state.reservation = reservation;
  token_blocks_[token.value()] = block;
  return Result<SequenceReservation>::success(reservation);
}

Result<void> BlockSequenceTable::complete(FtlToken token, Generation generation, bool success,
                                          std::uint32_t failed_page) {
  const auto token_it = token_blocks_.find(token.value());
  if (token_it == token_blocks_.end()) return Result<void>::failure({ErrorCode::kOutOfRange,"unknown workflow reservation"});
  auto state_it = states_.find(token_it->second);
  if (state_it == states_.end() || !state_it->second.reservation) return Result<void>::failure({ErrorCode::kOutOfRange,"unknown workflow reservation"});
  auto& state = state_it->second;
  if (state.reservation->generation != generation) return Result<void>::failure({ErrorCode::kIntegrity,"stale workflow completion"});
  state.reservation.reset();
  token_blocks_.erase(token_it);
  if (!success) {
    state.mode = SequenceMode::ReplayRequired;
    state.failed_page = failed_page;
    state.replay_progress = 0;
  }
  return Result<void>::success();
}

Result<void> BlockSequenceTable::cancel(FtlToken token, Generation generation) {
  const auto token_it = token_blocks_.find(token.value());
  if (token_it == token_blocks_.end()) return Result<void>::failure({ErrorCode::kOutOfRange,"unknown workflow reservation"});
  auto& state = states_.at(token_it->second);
  if (!state.reservation || state.reservation->generation != generation) return Result<void>::failure({ErrorCode::kIntegrity,"stale workflow cancellation"});
  state.reservation.reset(); token_blocks_.erase(token_it);
  return Result<void>::success();
}

Result<void> BlockSequenceTable::advance_replay(FtlToken token, Generation generation) {
  const auto token_it = token_blocks_.find(token.value());
  if (token_it == token_blocks_.end()) return Result<void>::failure({ErrorCode::kOutOfRange,"unknown workflow reservation"});
  auto& state = states_.at(token_it->second);
  if (!state.reservation || state.reservation->generation != generation) return Result<void>::failure({ErrorCode::kIntegrity,"stale replay"});
  ++state.replay_progress;
  return Result<void>::success();
}

const BlockSequenceState* BlockSequenceTable::inspect(
    const BlockKey& block) const noexcept {
  const auto found = states_.find(block);
  return found == states_.end() ? nullptr : &found->second;
}

}  // namespace openhbf::ftl
