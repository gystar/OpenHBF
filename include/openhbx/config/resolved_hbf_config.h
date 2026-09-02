#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "openhbx/config/config_types.h"

namespace openhbx::config {

struct HbfGeometry {
  std::uint64_t host_channels;
  std::uint64_t core_dies;
  std::uint64_t dies_per_core;
  std::uint64_t banks_per_die;
  std::uint64_t blocks_per_bank;
  std::uint64_t pages_per_block;
  std::uint64_t page_bytes;
  std::uint64_t page_count;
  std::uint64_t capacity_bytes;
};

struct ComponentSelection {
  std::string host;
  std::string address;
  std::string controller;
  std::string interconnect;
  std::string media;
  std::string ras;
};

struct PhysicalBankSelection {
  std::uint64_t core_die{0};
  std::uint64_t die{0};
  std::uint64_t bank{0};
};

struct ChannelOwnershipConfig {
  bool synthetic{false};
  std::vector<std::vector<PhysicalBankSelection>> banks_by_channel;
};

struct ResolvedSystemModel {
  std::string source;
  std::uint64_t tck_picoseconds{1000};
  std::uint64_t completion_capacity{4096};
  std::uint64_t axi_interfaces{1};
  std::uint64_t host_queue_depth_per_interface{256};
  std::uint64_t controller_pending_dlu{64};
  std::uint64_t controller_accumulation_timeout{1000};
  std::uint64_t controller_queue_depth{256};
  std::uint64_t controller_cache_buffers_per_bank{2};
  std::uint64_t controller_ecc_credits{64};
  // Cycle-valued timing parameters use tck_picoseconds as their time base.
  // The synthetic baseline models tR=4 us and tPROG=75 us at tCK=1 ns.
  std::uint64_t controller_backend_timeout{100000};
  std::uint64_t pal_max_inflight{256};
  std::uint64_t pal_media_retry_budget{64};
  std::uint64_t pal_return_retry_budget{64};
  std::uint64_t pal_retry_delay_cycles{1};
  std::uint64_t media_max_inflight{256};
  std::uint64_t reliability_seed{1};
  std::uint64_t fabric_active_lanes{8};
  std::uint64_t fabric_spare_lanes{1};
  std::uint64_t fabric_bits_per_lane_cycle{8};
  std::uint64_t fabric_efficiency_ppm{1000000};
  std::uint64_t fabric_arbitration_cycles{1};
  std::uint64_t fabric_propagation_cycles{1};
  std::uint64_t media_read_command_cycles{1};
  std::uint64_t media_read_sense_cycles{4000};
  std::uint64_t media_program_data_cycles{2};
  std::uint64_t media_program_array_cycles{75000};
  std::uint64_t media_program_verify_cycles{3};
  // tERS has not been calibrated for this synthetic profile yet.
  std::uint64_t media_erase_setup_cycles{1};
  std::uint64_t media_erase_array_cycles{40};
  std::uint64_t media_erase_verify_cycles{4};
};

class ResolvedHbfConfig {
 public:
  ResolvedHbfConfig(std::string profile, HbfGeometry geometry,
                    ComponentSelection components,
                    ChannelOwnershipConfig ownership,
                    ResolvedSystemModel system_model,
                    std::map<std::string, SourceMetadata> sources,
                    std::string canonical, std::string hash);
  const std::string& profile() const noexcept { return profile_; }
  const HbfGeometry& geometry() const noexcept { return geometry_; }
  const ComponentSelection& components() const noexcept { return components_; }
  const ChannelOwnershipConfig& ownership() const noexcept { return ownership_; }
  const ResolvedSystemModel& system_model() const noexcept { return system_model_; }
  const std::map<std::string, SourceMetadata>& sources() const noexcept { return sources_; }
  const std::string& canonical() const noexcept { return canonical_; }
  const std::string& canonical_hash() const noexcept { return hash_; }

 private:
  std::string profile_;
  HbfGeometry geometry_;
  ComponentSelection components_;
  ChannelOwnershipConfig ownership_;
  ResolvedSystemModel system_model_;
  std::map<std::string, SourceMetadata> sources_;
  std::string canonical_;
  std::string hash_;
};

ConfigResult<ResolvedHbfConfig> resolve_hbf_config(const RawConfigTree& raw);

}  // namespace openhbx::config
