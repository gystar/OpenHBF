#include "openhbx/interconnect/fabric.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "openhbx/common/checked_math.h"

namespace openhbx::interconnect {

InterconnectFabric::InterconnectFabric(FabricProfile profile, TsvRepairManager& repair)
    : profile_(std::move(profile)), repair_(repair) {
  const auto error = profile_.validate();
  if (!error.empty()) throw std::invalid_argument(error);
}

std::optional<Route> InterconnectFabric::route_for(std::uint64_t endpoint) const {
  for (const auto& candidate : profile_.routes) {
    if (candidate.endpoint != endpoint) continue;
    Route route;
    if (!repair_.resolve(candidate, route)) return std::nullopt;
    return route;
  }
  return std::nullopt;
}


TransferReservation InterconnectFabric::try_reserve(const Transfer& transfer, Cycle now) {
  if (active_.size() >= profile_.queue_depth)
    return {ReservationCode::Busy, {}, {}, 0};
  if (transfer.id == 0 || transfer.bits == 0 || transfer.route.resources.empty() ||
      active_.count(transfer.id))
    return {ReservationCode::InvalidRoute, {}, {}, 0};
  const RouteProfile* known = nullptr;
  for (const auto& route : profile_.routes) {
    if (route.id == transfer.route.id)
      known = &route;
  }
  if (!known || known->resources != transfer.route.resources ||
      known->logical_lanes != transfer.route.logical_lanes ||
      transfer.route.physical_lanes.size() != transfer.route.logical_lanes.size())
    return {ReservationCode::InvalidRoute, {}, {}, 0};
  if (!repair_.is_operational(transfer.route))
    return {ReservationCode::PathUnavailable, {}, {}, transfer.route.repair_epoch};

//计算一个传输任务最早什么时候能开始，以及什么时候能够完成。
  std::vector<ResourceId> resources = transfer.route.resources;
  std::sort(resources.begin(), resources.end());
  resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
  Cycle start = now;
  auto& resource_eat = transfer.direction == Direction::Forward
                           ? forward_resource_eat_ : return_resource_eat_;
  for (auto resource : resources) {
    const auto found = resource_eat.find(resource);
    if (found != resource_eat.end() && start < found->second)
      start = found->second; //不断选择更晚的时间
  }
  const auto rate = profile_.effective_bits_per_cycle();
  if (rate == 0)
    return {ReservationCode::Overflow, {}, {}, transfer.route.repair_epoch};
  const auto rounded = checked_add(transfer.bits, rate - 1);
  if (!rounded)
    return {ReservationCode::Overflow, {}, {}, transfer.route.repair_epoch};
  const std::uint64_t transfer_cycles = *rounded / rate;
  auto end = checked_add(start.value(), profile_.arbitration_cycles);
  if (end) end = checked_add(*end, transfer_cycles);
  if (end) end = checked_add(*end, profile_.propagation_cycles);
  if (!end) return {ReservationCode::Overflow, {}, {}, transfer.route.repair_epoch};

  const Cycle end_cycle(*end);
  for (auto resource : resources)
    resource_eat[resource] = end_cycle;
  active_.emplace(transfer.id, end_cycle);
  ++reservation_count_;
  return {ReservationCode::Accepted, start, end_cycle, transfer.route.repair_epoch};
}

bool InterconnectFabric::release(TransferId id) { return active_.erase(id) == 1; }

bool InterconnectFabric::route_is_operational(const Route& route) const {
  return repair_.is_operational(route);
}

void InterconnectFabric::reset() {
  active_.clear();
  forward_resource_eat_.clear();
  return_resource_eat_.clear();
}


FabricSnapshot InterconnectFabric::snapshot() const {
  std::map<ResourceId, Cycle> combined = forward_resource_eat_;
  for (const auto& item : return_resource_eat_) {
    auto found = combined.find(item.first);
    if (found == combined.end() || found->second < item.second)
      combined[item.first] = item.second;
  }
  //取每个资源取两个方向中更晚的时间
  return {active_.size(), reservation_count_, std::move(combined)};
}

}  // namespace openhbx::interconnect
