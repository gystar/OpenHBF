#include "openhbf/integration/open_hbf_config.h"

#include <sstream>

namespace openhbf::integration {
namespace {
Result<void> bad(const std::string& path, std::uint64_t value,
                 const std::string& constraint) {
  return Result<void>::failure(
      {ErrorCode::kInvalidArgument, path + "=" + std::to_string(value) +
                                         " " + constraint});
}
}  // namespace

OpenHbfConfig OpenHbfConfig::defaults() {
  OpenHbfConfig config;
  config.capabilities.functional_payload = false;
  config.capabilities = config.manifest();
  return config;
}

Result<void> OpenHbfConfig::validate() const {
  if (clock.ratio == 0) return bad("clock.ratio", 0, "must be non-zero");
  if (clock.tck_picoseconds == 0)
    return bad("clock.tck_picoseconds", 0, "must be non-zero");
  if (host.channels == 0 || host.channels > 16)
    return bad("host.channels", host.channels, "must be in [1,16]");
  if (host.axi_interfaces_per_channel != 1 &&
      host.axi_interfaces_per_channel != 2 &&
      host.axi_interfaces_per_channel != 4)
    return bad("host.axi_interfaces_per_channel",
               host.axi_interfaces_per_channel, "must be 1, 2, or 4");
  if (host.queue_depth_per_channel == 0)
    return bad("host.queue_depth_per_channel", 0, "must be non-zero");
  if (host.transaction_bytes != 64)
    return bad("host.transaction_bytes", host.transaction_bytes,
               "must be 64 bytes for the OCP profile");
  if (controller.max_pending_dlus_per_channel == 0)
    return bad("controller.max_pending_dlus_per_channel", 0,
               "must be non-zero");
  if (controller.accumulation_timeout_cycles == 0)
    return bad("controller.accumulation_timeout_cycles", 0,
               "must be non-zero cycles");
  if (geometry.core_dies == 0 || geometry.dies_per_core == 0 ||
      geometry.banks_per_die == 0 || geometry.blocks_per_bank == 0 ||
      geometry.pages_per_block == 0)
    return Result<void>::failure({ErrorCode::kInvalidArgument,
                                  "geometry dimensions must all be non-zero"});
  if (media_max_in_flight == 0)
    return bad("media.max_in_flight", 0, "must be non-zero");
  if (capabilities.channels != host.channels)
    return bad("capabilities.channels", capabilities.channels,
               "must equal host.channels=" + std::to_string(host.channels));
  if (capabilities.transaction_bytes != host.transaction_bytes)
    return bad("capabilities.transaction_bytes", capabilities.transaction_bytes,
               "must equal host.transaction_bytes");
  if (capabilities.dlu_bytes != 4096)
    return bad("capabilities.dlu_bytes", capabilities.dlu_bytes,
               "must be 4096 bytes");
  if (capabilities.active_data_gc)
    return bad("capabilities.active_data_gc", 1,
               "must be false for the OCP Host-controlled profile");
  if (capabilities.functional_payload !=
      (payload_mode == PayloadMode::Functional))
    return bad("capabilities.functional_payload",
               capabilities.functional_payload ? 1 : 0,
               "must match payload_mode");
  return Result<void>::success();
}

CapabilityManifest OpenHbfConfig::manifest() const noexcept {
  CapabilityManifest result = capabilities;
  result.channels = host.channels;
  result.transaction_bytes = host.transaction_bytes;
  result.dlu_bytes = 4096;
  result.active_data_gc = false;
  result.functional_payload = payload_mode == PayloadMode::Functional;
  return result;
}

std::string OpenHbfConfig::canonical_dump() const {
  std::ostringstream out;
  out << "clock.ratio=" << clock.ratio << '\n'
      << "clock.tck_picoseconds=" << clock.tck_picoseconds << '\n'
      << "controller.accumulation_timeout_cycles="
      << controller.accumulation_timeout_cycles << '\n'
      << "controller.max_pending_dlus_per_channel="
      << controller.max_pending_dlus_per_channel << '\n'
      << "geometry.banks_per_die=" << geometry.banks_per_die << '\n'
      << "geometry.blocks_per_bank=" << geometry.blocks_per_bank << '\n'
      << "geometry.core_dies=" << geometry.core_dies << '\n'
      << "geometry.dies_per_core=" << geometry.dies_per_core << '\n'
      << "geometry.pages_per_block=" << geometry.pages_per_block << '\n'
      << "host.axi_interfaces_per_channel="
      << host.axi_interfaces_per_channel << '\n'
      << "host.channels=" << host.channels << '\n'
      << "host.queue_depth_per_channel=" << host.queue_depth_per_channel << '\n'
      << "host.transaction_bytes=" << host.transaction_bytes << '\n'
      << "media.max_in_flight=" << media_max_in_flight << '\n'
      << "payload_mode="
      << (payload_mode == PayloadMode::Functional ? "functional" : "timing_only")
      << '\n'
      << "seed=" << seed << '\n'
      << "capabilities.active_data_gc=" << capabilities.active_data_gc << '\n'
      << "capabilities.deterministic_faults="
      << capabilities.deterministic_faults << '\n'
      << "capabilities.host_zone_remap=" << capabilities.host_zone_remap << '\n'
      << "capabilities.functional_payload="
      << capabilities.functional_payload << '\n'
      << "capabilities.scratchpad=" << capabilities.scratchpad << '\n';
  return out.str();
}
}  // namespace openhbf::integration
