#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openhbx/common/payload_handle.h"
#include "openhbx/common/strong_types.h"
#include "openhbx/media/types.h"

namespace openhbx::interconnect {

using ResourceId = std::uint64_t;
using RouteId = std::uint64_t;
using TransferId = std::uint64_t;

enum class Direction { Forward, Return };
enum class TrafficClass { Command, ProgramData, ReadData, Status };

struct Route {
  RouteId id{0};
  std::vector<ResourceId> resources;
  std::vector<std::uint64_t> logical_lanes;
  std::vector<std::uint64_t> physical_lanes;
  std::uint64_t repair_epoch{0};
};

struct Transfer {
  TransferId id{0};
  Direction direction{Direction::Forward};
  TrafficClass traffic_class{TrafficClass::Command};
  std::uint64_t bits{0};
  Route route;
};

enum class ReservationCode { Accepted, Busy, InvalidRoute, PathUnavailable, Overflow };

struct TransferReservation {
  ReservationCode code{ReservationCode::InvalidRoute};
  Cycle start;
  Cycle end;
  std::uint64_t repair_epoch{0};
};

}  // namespace openhbx::interconnect
