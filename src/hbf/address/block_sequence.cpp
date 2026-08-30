#include "openhbx/hbf/address/block_sequence.h"

#include "openhbx/common/checked_math.h"

namespace openhbx::hbf::address {

bool BlockSequence::valid(const BlockKey& block) const noexcept {
  return block.channel.value() < geometry_.channels() &&
         block.owned_bank.value() < geometry_.owned_banks_per_channel() &&
         block.block.value() < geometry_.blocks_per_bank();
}

BlockSequence::State& BlockSequence::state_for(const BlockKey& block) {
  return states_[block];
}

ReserveResult BlockSequence::reserve_program(BlockKey block, PageIndex page, Token token) {
  if (!valid(block) || page.value() >= geometry_.r3()) {
    return {std::nullopt, SequenceError::OutOfRange, false};
  }
  auto& state = state_for(block);
  if (state.stable_mode == SequenceMode::Retired) {
    return {std::nullopt, SequenceError::Retired, false};
  }
  if (state.outstanding) return {std::nullopt, SequenceError::Busy, false};
  if (state.stable_mode == SequenceMode::Full) {
    return {std::nullopt, SequenceError::OrderViolation, false};
  }
  const bool replay = state.stable_mode == SequenceMode::Replay;
  const std::uint64_t expected = replay ? state.replay_cursor : state.expected_page;
  if (page.value() != expected) {
    return {std::nullopt,
            replay ? SequenceError::ReplayRequired : SequenceError::OrderViolation, false};
  }
  ProgramReservation reservation{block, page, token, state.epoch, replay};
  state.outstanding = reservation;
  const bool auto_erase = !replay && state.stable_mode == SequenceMode::Empty && page.value() == 0;
  return {reservation, SequenceError::None, auto_erase};
}

CompleteResult BlockSequence::complete_program(const ProgramReservation& reservation,
                                               ProgramResult result) {
  const auto it = states_.find(reservation.block);
  if (it == states_.end() || !it->second.outstanding) return {SequenceError::Stale};
  auto& state = it->second;
  const auto& current = *state.outstanding;
  if (current.epoch != reservation.epoch || current.token != reservation.token ||
      current.page != reservation.page || current.replay != reservation.replay) {
    return {SequenceError::Stale};
  }
  state.outstanding.reset();
  ++state.epoch;
  if (result == ProgramResult::Failure) {
    state.failure_page = reservation.page.value();
    state.replay_cursor = 0;
    state.stable_mode = SequenceMode::Replay;
    return {};
  }
  if (reservation.replay) {
    ++state.replay_cursor;
    if (state.failure_page && state.replay_cursor > *state.failure_page) {
      state.expected_page = *state.failure_page + 1;
      state.failure_page.reset();
      state.replay_cursor = 0;
      state.stable_mode = state.expected_page == geometry_.r3()
                              ? SequenceMode::Full : SequenceMode::Sequential;
    }
  } else {
    state.expected_page = reservation.page.value() + 1;
    state.stable_mode = state.expected_page == geometry_.r3()
                            ? SequenceMode::Full : SequenceMode::Sequential;
  }
  return {};
}

CompleteResult BlockSequence::cancel_program(const ProgramReservation& reservation) {
  const auto it = states_.find(reservation.block);
  if (it == states_.end() || !it->second.outstanding) return {SequenceError::Stale};
  auto& state = it->second;
  const auto& current = *state.outstanding;
  if (current.epoch != reservation.epoch || current.token != reservation.token) {
    return {SequenceError::Stale};
  }
  state.outstanding.reset();
  ++state.epoch;
  return {};
}

std::vector<LocalByteAddress> BlockSequence::replay_addresses(BlockKey block) const {
  std::vector<LocalByteAddress> addresses;
  const auto it = states_.find(block);
  if (it == states_.end() || it->second.stable_mode != SequenceMode::Replay ||
      !it->second.failure_page || !valid(block)) return addresses;
  const auto bank_span = checked_mul(geometry_.r1(), geometry_.r2());
  const auto block_pages = bank_span ? checked_mul(*bank_span, geometry_.r3()) : std::nullopt;
  const auto block_base = block_pages ? checked_mul(block.block.value(), *block_pages)
                                      : std::nullopt;
  const auto l2 = block_base ? checked_add(*block_base, block.owned_bank.value())
                             : std::nullopt;
  if (!l2) return {};
  for (std::uint64_t page = 0; page <= *it->second.failure_page; ++page) {
    const auto offset = checked_mul(page, geometry_.r5());
    const auto l1 = offset ? checked_add(*l2, *offset) : std::nullopt;
    auto a1 = l1 ? checked_mul(*l1, geometry_.r4()) : std::nullopt;
    const auto bytes = a1 ? checked_mul(*a1, 64) : std::nullopt;
    if (!bytes) return {};
    addresses.emplace_back(*bytes);
  }
  return addresses;
}

BlockSequenceSnapshot BlockSequence::snapshot(BlockKey block) const {
  const auto it = states_.find(block);
  if (it == states_.end()) return {};
  const auto& state = it->second;
  return {state.outstanding ? SequenceMode::Reserved : state.stable_mode,
          state.expected_page, state.failure_page, state.epoch,
          state.outstanding.has_value()};
}

bool BlockSequence::has_outstanding_for_block(BlockIndex block) const noexcept {
  for (const auto& entry : states_) {
    if (entry.first.block == block && entry.second.outstanding) return true;
  }
  return false;
}

void BlockSequence::retire(BlockKey block) {
  auto& state = state_for(block);
  state.outstanding.reset();
  state.stable_mode = SequenceMode::Retired;
  ++state.epoch;
}

void BlockSequence::reset() {
  for (auto& entry : states_) {
    entry.second.outstanding.reset();
    ++entry.second.epoch;
  }
}

}  // namespace openhbx::hbf::address
