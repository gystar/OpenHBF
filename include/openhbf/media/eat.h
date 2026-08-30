#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/common/types.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

enum class ResourceKind : std::uint8_t {
  BankArray,
  ChannelMediaPath,
  VendorResource,
};

struct StageSpec {
  StageKind kind = StageKind::ReadSense;
  Duration duration{};
  std::vector<ResourceId> resources;
};

struct Reservation {
  Cycle start{};
  Cycle end{};
  std::vector<ResourceId> resources;
};

struct EatEntry {
  ResourceId resource{};
  Cycle next_available{};
  std::uint64_t version = 0;
};

// BankArray is a physical array identity and excludes Host Channel.
// ChannelMediaPath is a synthetic transfer contention domain identified by
// (owner Channel, physical Core Die domain); it does not add Media capacity.
Result<ResourceId> bank_array_resource(const MediaAddress& address);
Result<ResourceId> channel_media_path_resource(const MediaAddress& address);
Result<ResourceId> vendor_resource(std::uint64_t vendor_id);

class EatTable {
 public:
  explicit EatTable(std::vector<ResourceId> resources);

  Result<Reservation> preview(const std::vector<ResourceId>& resources,
                              Cycle now, Duration duration) const;
  Result<Reservation> reserve_atomic(
      const std::vector<ResourceId>& resources, Cycle now, Duration duration);
  Result<Reservation> reserve_atomic(MediaToken owner,
      const std::vector<ResourceId>& resources, Cycle now, Duration duration);
  std::size_t cancel(MediaToken owner, Cycle now) noexcept;
  std::vector<EatEntry> snapshot() const;

 private:
  struct State {
    Cycle next_available{};
    std::uint64_t version = 0;
    struct Interval { MediaToken owner; Cycle start; Cycle end; };
    std::vector<Interval> reservations;
  };

  std::unordered_map<ResourceId, State> entries_;
};

}  // namespace openhbf::media
