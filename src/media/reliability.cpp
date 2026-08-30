#include "openhbf/media/reliability.h"

#include <cmath>
#include <limits>

namespace openhbf::media {
namespace {

constexpr ReliabilityParams kDefaultSlc =
    ReliabilityParams::make(100000, 1e-8, 1e-10, 1e-8);
constexpr ReliabilityParams kDefaultMlc =
    ReliabilityParams::make(30000, 1e-7, 1e-9, 1e-7);
constexpr ReliabilityParams kDefaultTlc =
    ReliabilityParams::make(3000, 1e-6, 1e-8, 1e-6);
constexpr ReliabilityParams kDefaultQlc =
    ReliabilityParams::make(1000, 1e-5, 1e-7, 1e-5);

std::uint64_t mix(std::uint64_t value) noexcept {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

void combine(std::uint64_t& hash, std::uint64_t value) noexcept {
  hash = mix(hash ^ mix(value));
}

MediaAddress block_address(MediaAddress address) noexcept {
  address.page = 0;
  return address;
}

bool valid_rate(double value) noexcept {
  return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

}  // namespace

ReliabilityModel::ReliabilityModel(ReliabilityParams params,
                                   ReliabilityConfig config,
                                   std::vector<FaultRule> rules)
    : params_(params), config_(config), rules_(std::move(rules)) {}

Result<ReliabilityModel> ReliabilityModel::make_defaults(
    CellMode mode, ReliabilityConfig config, std::vector<FaultRule> rules) {
  switch (mode) {
    case CellMode::Slc: return create(kDefaultSlc, config, std::move(rules));
    case CellMode::Mlc: return create(kDefaultMlc, config, std::move(rules));
    case CellMode::Tlc: return create(kDefaultTlc, config, std::move(rules));
    case CellMode::Qlc: return create(kDefaultQlc, config, std::move(rules));
  }
  return Result<ReliabilityModel>::failure(
      {ErrorCode::kInvalidArgument, "unknown NAND cell mode"});
}

Result<ReliabilityModel> ReliabilityModel::create(
    ReliabilityParams params, ReliabilityConfig config,
    std::vector<FaultRule> rules) {
  if (params.max_pe_cycles == 0 || !valid_rate(params.raw_bit_error_rate) ||
      !valid_rate(params.read_disturb_rate) ||
      !valid_rate(params.data_retention_rate)) {
    return Result<ReliabilityModel>::failure(
        {ErrorCode::kInvalidArgument,
         "reliability max PE must be non-zero and rates must be in [0,1]"});
  }
  return Result<ReliabilityModel>::success(
      ReliabilityModel(params, config, std::move(rules)));
}

const FaultRule* ReliabilityModel::matching_rule(
    const ReliabilityInput& input, FaultKind kind) const noexcept {
  for (const FaultRule& rule : rules_) {
    if (!rule.enabled || rule.kind != kind) continue;
    if (rule.token && *rule.token != input.token) continue;
    if (rule.address && *rule.address != input.address) continue;
    if (rule.stage && *rule.stage != input.stage) continue;
    return &rule;
  }
  return nullptr;
}

bool ReliabilityModel::sample(const ReliabilityInput& input, FaultKind kind,
                              double probability) const noexcept {
  if (probability <= 0.0) return false;
  if (probability >= 1.0) return true;
  std::uint64_t hash = config_.seed;
  combine(hash, input.token.value());
  combine(hash, input.address.channel.value());
  combine(hash, input.address.core_die);
  combine(hash, input.address.die);
  combine(hash, input.address.bank);
  combine(hash, input.address.block);
  combine(hash, input.address.page);
  combine(hash, static_cast<std::uint8_t>(input.op));
  combine(hash, static_cast<std::uint8_t>(input.stage));
  combine(hash, static_cast<std::uint8_t>(kind));
  const long double unit = static_cast<long double>(hash) /
      static_cast<long double>(std::numeric_limits<std::uint64_t>::max());
  return unit < probability;
}

RawReadResult ReliabilityModel::evaluate_read(
    const ReliabilityInput& input) const noexcept {
  if (const FaultRule* rule = matching_rule(input, FaultKind::ReadErrors)) {
    const std::uint32_t errors = rule->raw_errors;
    return {errors,
            config_.retry_error_threshold != 0 &&
                errors >= config_.retry_error_threshold,
            config_.corrected_error_threshold != 0 &&
                errors > config_.corrected_error_threshold,
            true};
  }
  double probability = params_.raw_bit_error_rate;
  probability += params_.read_disturb_rate *
      static_cast<double>(input.read_count);
  probability += params_.data_retention_rate *
      static_cast<double>(input.program_age);
  const double wear = static_cast<double>(input.pe_cycles) /
      static_cast<double>(params_.max_pe_cycles);
  probability += wear * wear * 0.01;
  if (input.pe_cycles >= params_.max_pe_cycles) probability = 1.0;
  if (probability > 1.0) probability = 1.0;
  const std::uint32_t errors = sample(input, FaultKind::ReadErrors, probability) ? 1U : 0U;
  return {errors,
          config_.retry_error_threshold != 0 && errors >= config_.retry_error_threshold,
          config_.corrected_error_threshold != 0 &&
              errors > config_.corrected_error_threshold,
          false};
}

FailureDecision ReliabilityModel::evaluate_program(
    const ReliabilityInput& input) const noexcept {
  if (matching_rule(input, FaultKind::ProgramFailure)) return {true, true};
  const double ratio = static_cast<double>(input.pe_cycles) /
      static_cast<double>(params_.max_pe_cycles);
  const double wear = input.pe_cycles >= params_.max_pe_cycles
                          ? 1.0
                          : ratio * ratio * 1e-3;
  return {sample(input, FaultKind::ProgramFailure, wear), false};
}

FailureDecision ReliabilityModel::evaluate_erase(
    const ReliabilityInput& input) const noexcept {
  if (matching_rule(input, FaultKind::EraseFailure)) return {true, true};
  const double ratio = static_cast<double>(input.pe_cycles) /
      static_cast<double>(params_.max_pe_cycles);
  const double wear = input.pe_cycles >= params_.max_pe_cycles
                          ? 1.0
                          : ratio * ratio * 1e-3;
  return {sample(input, FaultKind::EraseFailure, wear), false};
}

std::optional<RefreshNotice> ReliabilityModel::on_successful_sense(
    const MediaAddress& address, std::uint64_t epoch,
    std::uint64_t read_count) const {
  if (config_.refresh_read_threshold != 0 &&
      read_count == config_.refresh_read_threshold) {
    return RefreshNotice{block_address(address), epoch, read_count};
  }
  return std::nullopt;
}

}  // namespace openhbf::media
