#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "openhbx/hbf/address/hbf_address_types.h"
#include "openhbx/hbf/address/hbf_geometry.h"

namespace openhbx::hbf::address {

enum class SequenceError {
  None,
  Busy,
  OutOfRange,
  OrderViolation,
  ReplayRequired,
  Stale,
  Duplicate,
  Retired,
  Overflow,
};
enum class SequenceMode { Empty, Sequential, Reserved, Replay, Full, Retired };
enum class ProgramResult { Success, Failure };

struct ProgramReservation {
  BlockKey block;
  PageIndex page;
  Token token;
  std::uint64_t epoch{0};
  bool replay{false};
};

struct ReserveResult {
  std::optional<ProgramReservation> reservation;
  SequenceError error{SequenceError::None};
  bool auto_erase_required{false};
  explicit operator bool() const noexcept { return reservation.has_value(); }
};

struct CompleteResult {
  SequenceError error{SequenceError::None};
  explicit operator bool() const noexcept { return error == SequenceError::None; }
};

struct BlockSequenceSnapshot {
  SequenceMode mode{SequenceMode::Empty};
  std::uint64_t expected_page{0};
  std::optional<std::uint64_t> failure_page;
  std::uint64_t epoch{0};
  bool outstanding{false};
};

class BlockSequence {
 public:
  explicit BlockSequence(const HbfGeometry& geometry) : geometry_(geometry) {}
  ReserveResult reserve_program(BlockKey block, PageIndex page, Token token);
  CompleteResult complete_program(const ProgramReservation& reservation,
                                  ProgramResult result);
  CompleteResult cancel_program(const ProgramReservation& reservation);
  std::vector<LocalByteAddress> replay_addresses(BlockKey block) const;
  BlockSequenceSnapshot snapshot(BlockKey block) const;
  bool has_outstanding_for_block(BlockIndex block) const noexcept;
  void retire(BlockKey block);
  void reset();

 private:
  struct State {
    SequenceMode stable_mode{SequenceMode::Empty};
    std::uint64_t expected_page{0};
    std::optional<std::uint64_t> failure_page;
    std::uint64_t replay_cursor{0};
    std::uint64_t epoch{0};
    std::optional<ProgramReservation> outstanding;
  };
  bool valid(const BlockKey& block) const noexcept;
  State& state_for(const BlockKey& block);
  const HbfGeometry& geometry_;
  std::map<BlockKey, State> states_;
};

}  // namespace openhbx::hbf::address
