#include "openhbf/media/die_environment.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "openhbf/common/checked_math.h"

namespace openhbf::media {

std::size_t DieKeyHash::operator()(const DieKey& key) const noexcept {
  std::size_t hash = static_cast<std::size_t>(key.core_die);
  hash ^= static_cast<std::size_t>(key.die) + 0x9e3779b9U + (hash << 6U) +
          (hash >> 2U);
  return hash;
}

DieEnvironment::DieEnvironment(ThermalConfig config, Generation generation)
    : config_(config), generation_(generation) {}

AcceptDecision DieEnvironment::can_accept(DieKey die, MediaOp) const {
  const auto it = dies_.find(die);
  const DieState state = it == dies_.end() ? DieState::Ready : it->second;
  if (thermal_mode_ == ThermalMode::Cattrip || state == DieState::Failed) {
    return {false, MediaStatus::DieFailed, DieState::Failed};
  }
  if (state == DieState::Recovering) {
    return {false, MediaStatus::DieRecovering, state};
  }
  return {true, MediaStatus::Success, state};
}

Result<StageModifier> DieEnvironment::stage_modifier(
    Duration base_duration) const {
  if (base_duration.value() == 0) {
    return Result<StageModifier>::failure(
        {ErrorCode::kInvalidArgument, "stage duration must be non-zero"});
  }
  if (thermal_mode_ != ThermalMode::Throttled) {
    return Result<StageModifier>::success(
        {base_duration, thermal_mode_});
  }
  if (config_.throttle_numerator == 0 || config_.throttle_denominator == 0) {
    return Result<StageModifier>::failure(
        {ErrorCode::kInvalidArgument,
         "thermal throttle ratio requires non-zero numerator and denominator"});
  }
  auto scaled = checked_mul(base_duration,
                            static_cast<std::uint64_t>(
                                config_.throttle_numerator));
  if (!scaled) return Result<StageModifier>::failure(scaled.error());
  auto rounded = checked_ceil_div(
      scaled.value(), static_cast<std::uint64_t>(config_.throttle_denominator));
  if (!rounded) return Result<StageModifier>::failure(rounded.error());
  return Result<StageModifier>::success({rounded.value(), thermal_mode_});
}

Result<void> DieEnvironment::update_temperature(double celsius, Cycle cycle) {
  if (!std::isfinite(celsius)) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "temperature must be finite"});
  }
  if (!std::isfinite(config_.throttle_celsius) ||
      !std::isfinite(config_.cattrip_celsius) ||
      config_.throttle_celsius >= config_.cattrip_celsius) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "invalid thermal thresholds"});
  }
  if (cycle < last_update_) {
    return Result<void>::failure(
        {ErrorCode::kUnderflow, "temperature update cycle moved backwards"});
  }
  temperature_celsius_ = celsius;
  thermal_mode_ = classify(celsius);
  last_update_ = cycle;
  return Result<void>::success();
}

Result<void> DieEnvironment::set_die_state(DieKey die, DieState state) {
  const auto it = dies_.find(die);
  const DieState current = it == dies_.end() ? DieState::Ready : it->second;
  const bool valid =
      current == state ||
      (current == DieState::Ready && state == DieState::Recovering) ||
      (current == DieState::Recovering && state == DieState::Ready) ||
      (current == DieState::Recovering && state == DieState::Failed);
  if (!valid) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "illegal Die state transition"});
  }
  dies_[die] = state;
  return Result<void>::success();
}

Result<void> DieEnvironment::observe_path_fault(ResourceId path,
                                                PathFaultKind kind,
                                                Cycle cycle,
                                                Generation generation) {
  if (generation != generation_) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "stale path fault generation"});
  }
  if (cycle < last_update_) {
    return Result<void>::failure(
        {ErrorCode::kUnderflow, "path observation cycle moved backwards"});
  }
  auto it = path_faults_.find(path);
  if (it != path_faults_.end() && cycle < it->second.cycle) {
    return Result<void>::failure(
        {ErrorCode::kUnderflow, "path observation cycle moved backwards"});
  }
  path_faults_[path] = {path, kind, cycle, generation};
  last_update_ = cycle;
  return Result<void>::success();
}

void DieEnvironment::reset_generation(Generation generation) noexcept {
  generation_ = generation;
  path_faults_.clear();
}

DieEnvironmentSnapshot DieEnvironment::snapshot() const {
  DieEnvironmentSnapshot result;
  result.temperature_celsius = temperature_celsius_;
  result.thermal_mode = thermal_mode_;
  result.last_update = last_update_;
  result.generation = generation_;
  for (const auto& die : dies_) result.dies.push_back(die);
  for (const auto& fault : path_faults_) result.path_faults.push_back(fault.second);
  std::sort(result.dies.begin(), result.dies.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.first.core_die != rhs.first.core_die)
                return lhs.first.core_die < rhs.first.core_die;
              return lhs.first.die < rhs.first.die;
            });
  std::sort(result.path_faults.begin(), result.path_faults.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.path < rhs.path;
            });
  return result;
}

ThermalMode DieEnvironment::classify(double celsius) const noexcept {
  if (celsius >= config_.cattrip_celsius) return ThermalMode::Cattrip;
  if (celsius >= config_.throttle_celsius) return ThermalMode::Throttled;
  return ThermalMode::Normal;
}

}  // namespace openhbf::media
