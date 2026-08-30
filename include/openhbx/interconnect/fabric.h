#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

#include "openhbx/interconnect/fabric_profile.h"
#include "openhbx/interconnect/tsv_repair.h"

namespace openhbx::interconnect {

struct FabricSnapshot {
  std::size_t active_reservations{0};
  std::uint64_t reservation_count{0};
  std::map<ResourceId, Cycle> resource_eat;
};

class InterconnectFabric {
 public:
  InterconnectFabric(FabricProfile profile, TsvRepairManager& repair);
  std::optional<Route> route_for(std::uint64_t endpoint) const;
  TransferReservation try_reserve(const Transfer& transfer, Cycle now);
  bool release(TransferId id);
  bool route_is_operational(const Route& route) const;
  void reset();
  FabricSnapshot snapshot() const;
  const FabricProfile& profile() const noexcept { return profile_; }

 private:
  FabricProfile profile_;
  TsvRepairManager& repair_;
  std::map<ResourceId, Cycle> forward_resource_eat_;
  std::map<ResourceId, Cycle> return_resource_eat_;
  std::map<TransferId, Cycle> active_;
  std::uint64_t reservation_count_{0};
};

}  // namespace openhbx::interconnect
