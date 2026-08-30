#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/common/types.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

struct DieKey {
  std::uint16_t core_die = 0;
  std::uint16_t die = 0;

  friend bool operator==(const DieKey& lhs, const DieKey& rhs) noexcept {
    return lhs.core_die == rhs.core_die && lhs.die == rhs.die;
  }
};

struct DieKeyHash {
  std::size_t operator()(const DieKey& key) const noexcept;
};

enum class DieState : std::uint8_t { Ready, Recovering, Failed };
enum class ThermalMode : std::uint8_t { Normal, Throttled, Cattrip };
enum class PathFaultKind : std::uint8_t { Transient, Persistent };

struct AcceptDecision {
  bool accepted = false;
  MediaStatus status = MediaStatus::InternalError;
  DieState state = DieState::Ready;
};

struct StageModifier {
  Duration duration{};
  ThermalMode thermal_mode = ThermalMode::Normal;
};

struct PathFaultObservation {
  ResourceId path{};
  PathFaultKind kind = PathFaultKind::Transient;
  Cycle cycle{};
  Generation generation{};
};

struct DieEnvironmentSnapshot {
  double temperature_celsius = 0.0;
  ThermalMode thermal_mode = ThermalMode::Normal;
  Cycle last_update{};
  Generation generation{};
  std::vector<std::pair<DieKey, DieState>> dies;
  std::vector<PathFaultObservation> path_faults;
};

class DieEnvironment {
 public:
  explicit DieEnvironment(ThermalConfig config,
                          Generation generation = Generation{});

  AcceptDecision can_accept(DieKey die, MediaOp op) const;
  Result<StageModifier> stage_modifier(Duration base_duration) const;
  Result<void> update_temperature(double celsius, Cycle cycle);
  Result<void> set_die_state(DieKey die, DieState state);
  Result<void> observe_path_fault(ResourceId path, PathFaultKind kind,
                                  Cycle cycle, Generation generation);
  void reset_generation(Generation generation) noexcept;
  DieEnvironmentSnapshot snapshot() const;

 private:
  ThermalMode classify(double celsius) const noexcept;

  ThermalConfig config_;
  double temperature_celsius_ = 0.0;
  ThermalMode thermal_mode_ = ThermalMode::Normal;
  Cycle last_update_{};
  Generation generation_{};
  std::unordered_map<DieKey, DieState, DieKeyHash> dies_;
  std::unordered_map<ResourceId, PathFaultObservation> path_faults_;
};

}  // namespace openhbf::media
