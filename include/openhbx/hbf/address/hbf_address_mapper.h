#pragma once

#include "openhbx/hbf/address/channel_topology.h"
#include "openhbx/hbf/address/zone_map.h"

namespace openhbx::hbf::address {

class HbfAddressMapper {
 public:
  HbfAddressMapper(const HbfGeometry& geometry, const ChannelTopology& topology,
                   const ZoneMap* zones = nullptr)
      : geometry_(geometry), topology_(topology), zones_(zones) {}
  MapResult map(ChannelId channel, LocalByteAddress address) const;
  ReverseResult reverse(const HbfAddress& address) const;

 private:
  const HbfGeometry& geometry_;
  const ChannelTopology& topology_;
  const ZoneMap* zones_;
};

}  // namespace openhbx::hbf::address
