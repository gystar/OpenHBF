#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "openhbf/common/error.h"
#include "openhbf/integration/host_request.h"

namespace openhbf::integration {

struct ClockConfig {
  std::uint32_t ratio = 1;
  std::uint32_t tck_picoseconds = 1000;
};
struct HostConfig {
  std::uint16_t channels = 1;
  std::uint16_t axi_interfaces_per_channel = 1;
  std::size_t queue_depth_per_channel = 256;
  std::uint32_t transaction_bytes = 64;
};
struct ControllerConfig {
  std::size_t max_pending_dlus_per_channel = 64;
  std::uint64_t accumulation_timeout_cycles = 10000;
};
struct GeometryConfig {
  std::uint16_t core_dies = 1;
  std::uint16_t dies_per_core = 1;
  std::uint16_t banks_per_die = 1;
  std::uint32_t blocks_per_bank = 1;
  std::uint32_t pages_per_block = 1;
};
struct CapabilityManifest {
  std::uint16_t channels = 1;
  std::uint32_t transaction_bytes = 64;
  std::uint32_t dlu_bytes = 4096;
  bool scratchpad = false;
  bool host_zone_remap = true;
  bool active_data_gc = false;
  bool deterministic_faults = true;
  bool functional_payload = false;
};

struct OpenHbfConfig {
  ClockConfig clock;
  HostConfig host;
  ControllerConfig controller;
  GeometryConfig geometry;
  std::size_t media_max_in_flight = 256;
  std::uint64_t seed = 1;
  PayloadMode payload_mode = PayloadMode::TimingOnly;
  CapabilityManifest capabilities;

  static OpenHbfConfig defaults();
  Result<void> validate() const;
  CapabilityManifest manifest() const noexcept;
  std::string canonical_dump() const;
};

}  // namespace openhbf::integration
