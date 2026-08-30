#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

// These are synthetic research defaults, not OCP device guarantees. Rates are
// probabilities evaluated once for the corresponding command.
struct ReliabilityParams {
  std::uint64_t max_pe_cycles = 0;
  double raw_bit_error_rate = 0.0;
  double read_disturb_rate = 0.0;
  double data_retention_rate = 0.0;

  static constexpr ReliabilityParams make(std::uint64_t max_pe,
                                           double raw, double disturb,
                                           double retention) noexcept {
    return {max_pe, raw, disturb, retention};
  }
};

enum class FaultKind : std::uint8_t {
  ReadErrors,
  ProgramFailure,
  EraseFailure,
};

// A rule matches exactly the fields it supplies. Rules are checked in vector
// order, making an explicit test fault deterministic and stronger than rates.
struct FaultRule {
  FaultKind kind = FaultKind::ReadErrors;
  std::optional<MediaToken> token;
  std::optional<MediaAddress> address;
  std::optional<StageKind> stage;
  std::uint32_t raw_errors = 1;
  bool enabled = true;
};

struct ReliabilityInput {
  MediaToken token{};
  MediaAddress address;
  MediaOp op = MediaOp::Read;
  StageKind stage = StageKind::ReadSense;
  std::uint64_t block_epoch = 0;
  std::uint64_t pe_cycles = 0;
  std::uint64_t program_age = 0;
  std::uint64_t read_count = 0;
  double temperature_celsius = 25.0;
};

struct RawReadResult {
  std::uint32_t raw_errors = 0;
  bool retry_recommended = false;
  bool uncorrectable = false;
  bool explicit_fault = false;
};

struct FailureDecision {
  bool failed = false;
  bool explicit_fault = false;
};

struct RefreshNotice {
  MediaAddress block_address;
  std::uint64_t block_epoch = 0;
  std::uint64_t read_count = 0;
};

class ReliabilityModel {
 public:
  static Result<ReliabilityModel> make_defaults(
      CellMode mode, ReliabilityConfig config,
      std::vector<FaultRule> rules = {});
  static Result<ReliabilityModel> create(
      ReliabilityParams params, ReliabilityConfig config,
      std::vector<FaultRule> rules = {});

  RawReadResult evaluate_read(const ReliabilityInput& input) const noexcept;
  FailureDecision evaluate_program(
      const ReliabilityInput& input) const noexcept;
  FailureDecision evaluate_erase(const ReliabilityInput& input) const noexcept;

  // Only CommandEngine's successful Sense/Erase terminal paths call these.
  std::optional<RefreshNotice> on_successful_sense(
      const MediaAddress& address, std::uint64_t epoch,
      std::uint64_t read_count) const;

 private:
  ReliabilityModel(ReliabilityParams params, ReliabilityConfig config,
                   std::vector<FaultRule> rules);
  const FaultRule* matching_rule(const ReliabilityInput& input,
                                 FaultKind kind) const noexcept;
  bool sample(const ReliabilityInput& input, FaultKind kind,
              double probability) const noexcept;

  ReliabilityParams params_;
  ReliabilityConfig config_;
  std::vector<FaultRule> rules_;
};

}  // namespace openhbf::media
