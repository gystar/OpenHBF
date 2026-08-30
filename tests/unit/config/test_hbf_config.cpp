#include <cassert>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/resolved_hbf_config.h"

using namespace openhbx::config;

namespace {
const char* kValid = R"(product:
  profile: OCP_HBF_0_7
geometry:
  source: synthetic
  host_channels: 4
  core_dies: 4
  dies_per_core: 2
  banks_per_die: 16
  blocks_per_bank: 1024
  pages_per_block: 256
  page_bytes: 4096 B
components:
  host: host
  address: address
  controller: controller
  interconnect: interconnect
  media: media
  ras: ras
)";

bool has(const std::vector<ConfigIssue>& issues, ConfigErrorCode code,
         const std::string& path) {
  for (const auto& value : issues)
    if (value.code == code && value.path == path) return true;
  return false;
}

ResolvedHbfConfig resolve_valid(const RawConfigTree& raw) {
  auto result = resolve_hbf_config(raw);
  assert(result);
  return result.take_value();
}

RawConfigTree small_tree(const std::string& source = "synthetic") {
  RawConfigTree raw;
  raw.scalars = {{"product.profile", "OCP_HBF_0_7"},
                 {"geometry.source", source},
                 {"geometry.host_channels", "2"},
                 {"geometry.core_dies", "1"},
                 {"geometry.dies_per_core", "1"},
                 {"geometry.banks_per_die", "4"},
                 {"geometry.blocks_per_bank", "1"},
                 {"geometry.pages_per_block", "1"},
                 {"geometry.page_bytes", "4096 B"},
                 {"components.host", "host"},
                 {"components.address", "address"},
                 {"components.controller", "controller"},
                 {"components.interconnect", "interconnect"},
                 {"components.media", "media"},
                 {"components.ras", "ras"}};
  return raw;
}
}

int main() {
  auto parsed = parse_hbf_yaml(kValid);
  assert(parsed);
  auto resolved = resolve_hbf_config(parsed.value());
  assert(resolved);
  assert(resolved.value().profile() == "OCP_HBF_0_7");
  assert(resolved.value().geometry().page_count == 4ULL * 2 * 16 * 1024 * 256);
  assert(resolved.value().sources().at("geometry.core_dies").origin == ConfigOrigin::Synthetic);
  assert(resolved.value().canonical_hash().find("openhbx-fnv1a64-v1:") == 0);

  auto unsupported_tree = parsed.value();
  unsupported_tree.scalars["product.profile"] = "HBM3";
  auto unsupported = resolve_hbf_config(unsupported_tree);
  assert(!unsupported && has(unsupported.issues(), ConfigErrorCode::UnsupportedProfile, "product.profile"));

  auto unknown_tree = parsed.value();
  unknown_tree.scalars["geometry.magic"] = "1";
  auto unknown = resolve_hbf_config(unknown_tree);
  assert(!unknown && has(unknown.issues(), ConfigErrorCode::UnknownKey, "geometry.magic"));

  auto unit_tree = parsed.value();
  unit_tree.scalars["geometry.page_bytes"] = "4096";
  auto unit = resolve_hbf_config(unit_tree);
  assert(!unit && has(unit.issues(), ConfigErrorCode::UnitRequired, "geometry.page_bytes"));

  auto locked_tree = parsed.value();
  locked_tree.scalars["host.transaction_bytes"] = "128 B";
  auto locked = resolve_hbf_config(locked_tree);
  assert(!locked && has(locked.issues(), ConfigErrorCode::LockedField, "host.transaction_bytes"));

  auto overflow_tree = parsed.value();
  overflow_tree.scalars["geometry.core_dies"] = std::to_string(std::numeric_limits<std::uint64_t>::max());
  auto overflow = resolve_hbf_config(overflow_tree);
  assert(!overflow && has(overflow.issues(), ConfigErrorCode::ArithmeticOverflow, "geometry"));

  const std::string reordered = R"(components:
  ras: ras
  media: media
  interconnect: interconnect
  controller: controller
  address: address
  host: host
geometry:
  page_bytes: 4096 B
  pages_per_block: 256
  blocks_per_bank: 1024
  banks_per_die: 16
  dies_per_core: 2
  core_dies: 4
  host_channels: 4
  source: synthetic
product:
  profile: OCP_HBF_0_7
)";
  auto reordered_parsed = parse_hbf_yaml(reordered);
  assert(reordered_parsed);
  auto reordered_resolved = resolve_hbf_config(reordered_parsed.value());
  assert(reordered_resolved);
  assert(reordered_resolved.value().canonical_hash() == resolved.value().canonical_hash());
  assert(reordered_resolved.value().sources().at("geometry.core_dies").origin == ConfigOrigin::Synthetic);

  const auto baseline_tree = small_tree();
  const auto baseline = resolve_valid(baseline_tree);
  const std::vector<std::pair<std::string, std::string>> model_variants{
      {"model.source", "test:model-v2"},
      {"model.completion_capacity", "4097"},
      {"model.axi_interfaces", "2"},
      {"model.host_queue_depth_per_interface", "257"},
      {"model.controller_pending_dlu", "65"},
      {"model.controller_accumulation_timeout", "1001"},
      {"model.controller_queue_depth", "257"},
      {"model.controller_cache_buffers_per_bank", "3"},
      {"model.controller_ecc_credits", "65"},
      {"model.controller_backend_timeout", "10001"},
      {"model.pal_max_inflight", "257"},
      {"model.pal_media_retry_budget", "65"},
      {"model.pal_return_retry_budget", "65"},
      {"model.pal_retry_delay_cycles", "2"},
      {"model.media_max_inflight", "257"},
      {"model.reliability_seed", "2"},
      {"model.fabric_active_lanes", "9"},
      {"model.fabric_spare_lanes", "2"},
      {"model.fabric_bits_per_lane_cycle", "9"},
      {"model.fabric_efficiency_ppm", "999999"},
      {"model.fabric_arbitration_cycles", "2"},
      {"model.fabric_propagation_cycles", "2"},
      {"model.media_read_command_cycles", "2"},
      {"model.media_read_sense_cycles", "11"},
      {"model.media_program_data_cycles", "3"},
      {"model.media_program_array_cycles", "21"},
      {"model.media_program_verify_cycles", "4"},
      {"model.media_erase_setup_cycles", "2"},
      {"model.media_erase_array_cycles", "41"},
      {"model.media_erase_verify_cycles", "5"}};
  for (const auto& variant : model_variants) {
    auto changed_tree = baseline_tree;
    changed_tree.scalars[variant.first] = variant.second;
    const auto changed = resolve_valid(changed_tree);
    assert(changed.canonical_hash() != baseline.canonical_hash());
    assert(changed.canonical().find("\n" + variant.first + "=") != std::string::npos);
  }

  auto unknown_model_tree = baseline_tree;
  unknown_model_tree.scalars["model.unrecognized_latency"] = "1";
  auto unknown_model = resolve_hbf_config(unknown_model_tree);
  assert(!unknown_model && has(unknown_model.issues(), ConfigErrorCode::UnknownKey,
                               "model.unrecognized_latency"));

  auto vendor_without_ownership = small_tree("vendor");
  auto missing_ownership = resolve_hbf_config(vendor_without_ownership);
  assert(!missing_ownership &&
         has(missing_ownership.issues(), ConfigErrorCode::MissingField,
             "topology.channel_ownership"));

  auto layered_vendor = small_tree("vendor");
  layered_vendor.scalars["geometry.dies_per_core"] = "2";
  layered_vendor.scalars["geometry.banks_per_die"] = "1";
  layered_vendor.scalars["topology.channel_ownership"] = "0/0/0;0/1/0";
  const auto layered = resolve_valid(layered_vendor);
  assert(layered.ownership().banks_by_channel.size() == 2);
  assert(layered.ownership().banks_by_channel[0][0].die == 0);
  assert(layered.ownership().banks_by_channel[1][0].die == 1);

  auto fewer_banks_than_channels = small_tree();
  fewer_banks_than_channels.scalars["geometry.host_channels"] = "4";
  fewer_banks_than_channels.scalars["geometry.dies_per_core"] = "2";
  fewer_banks_than_channels.scalars["geometry.banks_per_die"] = "1";
  auto insufficient = resolve_hbf_config(fewer_banks_than_channels);
  assert(!insufficient &&
         has(insufficient.issues(), ConfigErrorCode::OutOfRange,
             "geometry.host_channels"));

  auto uneven_banks = small_tree();
  uneven_banks.scalars["geometry.host_channels"] = "3";
  auto uneven = resolve_hbf_config(uneven_banks);
  assert(!uneven && has(uneven.issues(), ConfigErrorCode::OutOfRange,
                        "geometry.host_channels"));

  auto ownership_a_tree = small_tree("vendor");
  ownership_a_tree.scalars["topology.channel_ownership"] =
      "0/0/0,0/0/2;0/0/1,0/0/3";
  const auto ownership_a = resolve_valid(ownership_a_tree);
  assert(!ownership_a.ownership().synthetic);
  assert(ownership_a.ownership().banks_by_channel.size() == 2);
  assert(ownership_a.ownership().banks_by_channel[0][1].bank == 2);

  auto ownership_b_tree = ownership_a_tree;
  ownership_b_tree.scalars["topology.channel_ownership"] =
      "0/0/1,0/0/3;0/0/0,0/0/2";
  const auto ownership_b = resolve_valid(ownership_b_tree);
  assert(ownership_b.canonical_hash() != ownership_a.canonical_hash());
  assert(ownership_b.ownership().banks_by_channel[0][0].bank == 1);

  for (const auto& invalid_ownership : {
           std::string("0/0/0,0/0/0;0/0/1,0/0/2"),
           std::string("0/0/0;0/0/1,0/0/2"),
           std::string("0/0/0,0/0/4;0/0/1,0/0/2")}) {
    auto invalid_tree = small_tree("vendor");
    invalid_tree.scalars["topology.channel_ownership"] = invalid_ownership;
    auto invalid = resolve_hbf_config(invalid_tree);
    assert(!invalid && has(invalid.issues(), ConfigErrorCode::OutOfRange,
                           "topology.channel_ownership"));
  }

  auto duplicate = parse_hbf_yaml("product:\n  profile: OCP_HBF_0_7\n  profile: OCP_HBF_0_7\n");
  assert(!duplicate && has(duplicate.issues(), ConfigErrorCode::DuplicateKey, "product.profile"));
}
