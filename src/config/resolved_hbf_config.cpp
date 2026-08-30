#include "openhbx/config/resolved_hbf_config.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

#include "openhbx/common/checked_math.h"
#include "openhbx/config/hbf_product_profile.h"

namespace openhbx::config {
namespace {
const std::set<std::string> kAllowed{
    "product.family", "product.profile", "geometry.source",
    "geometry.host_channels", "geometry.core_dies", "geometry.dies_per_core",
    "geometry.banks_per_die", "geometry.blocks_per_bank",
    "geometry.pages_per_block", "geometry.page_bytes", "components.host",
    "components.address", "components.controller", "components.interconnect",
    "components.media", "components.ras", "topology.channel_ownership",
    "model.source", "model.tck_picoseconds", "model.completion_capacity", "model.axi_interfaces",
    "model.host_queue_depth_per_interface", "model.controller_pending_dlu",
    "model.controller_accumulation_timeout", "model.controller_queue_depth",
    "model.controller_cache_buffers_per_bank", "model.controller_ecc_credits",
    "model.controller_backend_timeout", "model.pal_max_inflight",
    "model.pal_media_retry_budget", "model.pal_return_retry_budget",
    "model.pal_retry_delay_cycles", "model.media_max_inflight",
    "model.reliability_seed", "model.fabric_active_lanes",
    "model.fabric_spare_lanes", "model.fabric_bits_per_lane_cycle",
    "model.fabric_efficiency_ppm", "model.fabric_arbitration_cycles",
    "model.fabric_propagation_cycles", "model.media_read_command_cycles",
    "model.media_read_sense_cycles", "model.media_program_data_cycles",
    "model.media_program_array_cycles", "model.media_program_verify_cycles",
    "model.media_erase_setup_cycles", "model.media_erase_array_cycles",
    "model.media_erase_verify_cycles"};

ConfigIssue issue(ConfigErrorCode code, std::string path, std::string message) {
  return {code, std::move(path), std::move(message), 0};
}

bool parse_count(const RawConfigTree& raw, const std::string& path,
                 std::uint64_t& output, std::vector<ConfigIssue>& issues,
                 bool allow_zero = false) {
  const auto it = raw.scalars.find(path);
  if (it == raw.scalars.end()) {
    issues.push_back(issue(ConfigErrorCode::MissingField, path, "required field is missing"));
    return false;
  }
  const std::string& value = it->second;
  if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); })) {
    issues.push_back(issue(ConfigErrorCode::TypeMismatch, path, "expected an unsigned count"));
    return false;
  }
  try {
    std::size_t consumed = 0;
    output = std::stoull(value, &consumed);
    if (consumed != value.size()) throw std::invalid_argument("suffix");
  } catch (...) {
    issues.push_back(issue(ConfigErrorCode::OutOfRange, path, "count is not representable"));
    return false;
  }
  if (output == 0 && !allow_zero) {
    issues.push_back(issue(ConfigErrorCode::OutOfRange, path, "count must be greater than zero"));
    return false;
  }
  return true;
}

void parse_optional_count(const RawConfigTree& raw, const std::string& path,
                          std::uint64_t& output, std::vector<ConfigIssue>& issues,
                          bool allow_zero = false) {
  if (raw.scalars.count(path) == 0) return;
  std::uint64_t value = 0;
  if (parse_count(raw, path, value, issues, allow_zero)) output = value;
}

bool parse_bytes(const RawConfigTree& raw, const std::string& path,
                 std::uint64_t& output, std::vector<ConfigIssue>& issues) {
  const auto it = raw.scalars.find(path);
  if (it == raw.scalars.end()) {
    issues.push_back(issue(ConfigErrorCode::MissingField, path, "required field is missing"));
    return false;
  }
  const std::string suffix = " B";
  if (it->second.size() <= suffix.size() ||
      it->second.substr(it->second.size() - suffix.size()) != suffix) {
    issues.push_back(issue(ConfigErrorCode::UnitRequired, path, "byte values require the explicit ' B' unit"));
    return false;
  }
  RawConfigTree count;
  count.scalars[path] = it->second.substr(0, it->second.size() - suffix.size());
  return parse_count(count, path, output, issues);
}

std::string hash64(const std::string& canonical) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : canonical) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << "openhbx-fnv1a64-v1:" << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}
}

ResolvedHbfConfig::ResolvedHbfConfig(
    std::string profile, HbfGeometry geometry, ComponentSelection components,
    ChannelOwnershipConfig ownership, ResolvedSystemModel system_model,
    std::map<std::string, SourceMetadata> sources, std::string canonical,
    std::string hash)
    : profile_(std::move(profile)), geometry_(geometry),
      components_(std::move(components)), ownership_(std::move(ownership)),
      system_model_(std::move(system_model)), sources_(std::move(sources)),
      canonical_(std::move(canonical)), hash_(std::move(hash)) {}

ConfigResult<ResolvedHbfConfig> resolve_hbf_config(const RawConfigTree& raw) {
  std::vector<ConfigIssue> issues;
  for (const auto& entry : raw.scalars)
    if (kAllowed.count(entry.first) == 0)
      issues.push_back(issue(ConfigErrorCode::UnknownKey, entry.first, "field is not declared by schema v1"));

  const auto profile_it = raw.scalars.find("product.profile");
  const std::string profile_name = profile_it == raw.scalars.end() ? "" : profile_it->second;
  auto profile = find_hbf_product_profile(profile_name);
  if (!profile) issues.insert(issues.end(), profile.issues().begin(), profile.issues().end());
  const auto family_it = raw.scalars.find("product.family");
  if (family_it != raw.scalars.end() && family_it->second != "HBF")
    issues.push_back(issue(ConfigErrorCode::LockedField, "product.family", "OCP_HBF_0_7 locks family to HBF"));
  for (const auto* locked : {"host.transaction_bytes", "controller.dlu_bytes"})
    if (raw.scalars.count(locked) != 0)
      issues.push_back(issue(ConfigErrorCode::LockedField, locked, "profile-locked field cannot be overridden"));

  HbfGeometry geometry{};
  parse_count(raw, "geometry.host_channels", geometry.host_channels, issues);
  parse_count(raw, "geometry.core_dies", geometry.core_dies, issues);
  parse_count(raw, "geometry.dies_per_core", geometry.dies_per_core, issues);
  parse_count(raw, "geometry.banks_per_die", geometry.banks_per_die, issues);
  parse_count(raw, "geometry.blocks_per_bank", geometry.blocks_per_bank, issues);
  parse_count(raw, "geometry.pages_per_block", geometry.pages_per_block, issues);
  parse_bytes(raw, "geometry.page_bytes", geometry.page_bytes, issues);
  if (geometry.host_channels > 16)
    issues.push_back(issue(ConfigErrorCode::OutOfRange, "geometry.host_channels", "must be in range 1..16"));
  const auto die_count = openhbx::checked_mul(geometry.core_dies,
                                              geometry.dies_per_core);
  const auto physical_banks = die_count
      ? openhbx::checked_mul(*die_count, geometry.banks_per_die)
      : std::nullopt;
  if (!physical_banks) {
    issues.push_back(issue(ConfigErrorCode::ArithmeticOverflow, "geometry",
                           "physical Bank count overflows uint64"));
  } else if (geometry.host_channels != 0 &&
             (*physical_banks < geometry.host_channels ||
              *physical_banks % geometry.host_channels != 0)) {
    issues.push_back(issue(ConfigErrorCode::OutOfRange, "geometry.host_channels",
                           "physical Banks must divide evenly across Host Channels"));
  }

  std::uint64_t pages = geometry.core_dies;
  for (const auto factor : {geometry.dies_per_core, geometry.banks_per_die,
                            geometry.blocks_per_bank, geometry.pages_per_block}) {
    const auto next = openhbx::checked_mul(pages, factor);
    if (!next) {
      issues.push_back(issue(ConfigErrorCode::ArithmeticOverflow, "geometry", "physical page count overflows uint64"));
      pages = 0;
      break;
    }
    pages = *next;
  }
  geometry.page_count = pages;
  const auto capacity = openhbx::checked_mul(pages, geometry.page_bytes);
  if (!capacity) issues.push_back(issue(ConfigErrorCode::ArithmeticOverflow, "geometry.capacity_bytes", "capacity overflows uint64"));
  else geometry.capacity_bytes = *capacity;

  ComponentSelection components;
  const std::array<std::pair<const char*, std::string*>, 6> component_fields{{
      {"components.host", &components.host}, {"components.address", &components.address},
      {"components.controller", &components.controller}, {"components.interconnect", &components.interconnect},
      {"components.media", &components.media}, {"components.ras", &components.ras}}};
  for (const auto& field : component_fields) {
    const auto it = raw.scalars.find(field.first);
    if (it == raw.scalars.end() || it->second.empty())
      issues.push_back(issue(ConfigErrorCode::MissingField, field.first, "component impl is required"));
    else *field.second = it->second;
  }
  const auto source_it = raw.scalars.find("geometry.source");
  ConfigOrigin geometry_origin = ConfigOrigin::User;
  std::string source_detail = "user configuration";
  if (source_it != raw.scalars.end()) {
    if (source_it->second == "vendor") { geometry_origin = ConfigOrigin::Vendor; source_detail = "vendor product parameter"; }
    else if (source_it->second == "synthetic") { geometry_origin = ConfigOrigin::Synthetic; source_detail = "synthetic test parameter"; }
    else issues.push_back(issue(ConfigErrorCode::OutOfRange, "geometry.source", "expected vendor or synthetic"));
  }

  ResolvedSystemModel model;
  const auto model_source = raw.scalars.find("model.source");
  if (model_source != raw.scalars.end()) model.source = model_source->second;
  else model.source = "open-hbx:baseline-model-v1";
  if (model.source.empty())
    issues.push_back(issue(ConfigErrorCode::OutOfRange, "model.source", "model source is required"));
#define OPENHBX_MODEL_COUNT(field) \
  parse_optional_count(raw, "model." #field, model.field, issues)
  OPENHBX_MODEL_COUNT(completion_capacity);
  OPENHBX_MODEL_COUNT(tck_picoseconds);
  OPENHBX_MODEL_COUNT(axi_interfaces);
  OPENHBX_MODEL_COUNT(host_queue_depth_per_interface);
  OPENHBX_MODEL_COUNT(controller_pending_dlu);
  OPENHBX_MODEL_COUNT(controller_accumulation_timeout);
  OPENHBX_MODEL_COUNT(controller_queue_depth);
  OPENHBX_MODEL_COUNT(controller_cache_buffers_per_bank);
  OPENHBX_MODEL_COUNT(controller_ecc_credits);
  OPENHBX_MODEL_COUNT(controller_backend_timeout);
  OPENHBX_MODEL_COUNT(pal_max_inflight);
  OPENHBX_MODEL_COUNT(pal_media_retry_budget);
  OPENHBX_MODEL_COUNT(pal_return_retry_budget);
  OPENHBX_MODEL_COUNT(pal_retry_delay_cycles);
  OPENHBX_MODEL_COUNT(media_max_inflight);
  parse_optional_count(raw, "model.reliability_seed", model.reliability_seed, issues, true);
  OPENHBX_MODEL_COUNT(fabric_active_lanes);
  parse_optional_count(raw, "model.fabric_spare_lanes", model.fabric_spare_lanes, issues, true);
  OPENHBX_MODEL_COUNT(fabric_bits_per_lane_cycle);
  OPENHBX_MODEL_COUNT(fabric_efficiency_ppm);
  OPENHBX_MODEL_COUNT(fabric_arbitration_cycles);
  OPENHBX_MODEL_COUNT(fabric_propagation_cycles);
  OPENHBX_MODEL_COUNT(media_read_command_cycles);
  OPENHBX_MODEL_COUNT(media_read_sense_cycles);
  OPENHBX_MODEL_COUNT(media_program_data_cycles);
  OPENHBX_MODEL_COUNT(media_program_array_cycles);
  OPENHBX_MODEL_COUNT(media_program_verify_cycles);
  OPENHBX_MODEL_COUNT(media_erase_setup_cycles);
  OPENHBX_MODEL_COUNT(media_erase_array_cycles);
  OPENHBX_MODEL_COUNT(media_erase_verify_cycles);
#undef OPENHBX_MODEL_COUNT
  if (model.axi_interfaces != 1 && model.axi_interfaces != 2 && model.axi_interfaces != 4)
    issues.push_back(issue(ConfigErrorCode::OutOfRange, "model.axi_interfaces", "expected 1, 2, or 4"));
  if (model.fabric_efficiency_ppm > 1000000)
    issues.push_back(issue(ConfigErrorCode::OutOfRange, "model.fabric_efficiency_ppm", "must not exceed 1000000"));

  ChannelOwnershipConfig ownership;
  ownership.synthetic = geometry_origin == ConfigOrigin::Synthetic;
  ownership.banks_by_channel.resize(static_cast<std::size_t>(geometry.host_channels));
  const auto ownership_it = raw.scalars.find("topology.channel_ownership");
  if (ownership.synthetic && ownership_it == raw.scalars.end() && issues.empty()) {
    std::uint64_t flat = 0;
    for (std::uint64_t core = 0; core < geometry.core_dies; ++core)
      for (std::uint64_t die = 0; die < geometry.dies_per_core; ++die)
        for (std::uint64_t bank = 0; bank < geometry.banks_per_die; ++bank, ++flat)
          ownership.banks_by_channel[flat % geometry.host_channels].push_back({core, die, bank});
  } else if (ownership_it == raw.scalars.end() && !ownership.synthetic) {
    issues.push_back(issue(ConfigErrorCode::MissingField, "topology.channel_ownership",
                           "vendor geometry requires an explicit ChannelOwnershipProfile"));
  } else {
    std::set<std::array<std::uint64_t, 3>> seen;
    std::istringstream channels(ownership_it->second);
    std::string channel_text;
    std::size_t channel = 0;
    while (std::getline(channels, channel_text, ';')) {
      std::istringstream banks(channel_text);
      std::string bank_text;
      while (std::getline(banks, bank_text, ',')) {
        std::replace(bank_text.begin(), bank_text.end(), '/', ' ');
        std::istringstream fields(bank_text);
        PhysicalBankSelection bank;
        std::string trailing;
        if (!(fields >> bank.core_die >> bank.die >> bank.bank) || channel >= ownership.banks_by_channel.size() ||
            (fields >> trailing) ||
            bank.core_die >= geometry.core_dies || bank.die >= geometry.dies_per_core ||
            bank.bank >= geometry.banks_per_die ||
            !seen.insert({bank.core_die, bank.die, bank.bank}).second) {
          issues.push_back(issue(ConfigErrorCode::OutOfRange, "topology.channel_ownership",
                                 "expected unique core/die/bank entries grouped by ';'"));
          break;
        }
        ownership.banks_by_channel[channel].push_back(bank);
      }
      ++channel;
    }
    const auto core_die_banks = openhbx::checked_mul(
        geometry.core_dies, geometry.dies_per_core);
    const auto expected_banks = core_die_banks
        ? openhbx::checked_mul(*core_die_banks, geometry.banks_per_die)
        : std::nullopt;
    if (!expected_banks || seen.size() != *expected_banks ||
        channel != ownership.banks_by_channel.size())
      issues.push_back(issue(ConfigErrorCode::OutOfRange, "topology.channel_ownership",
                             "ownership must cover every physical Bank exactly once"));
  }
  std::sort(issues.begin(), issues.end(), [](const ConfigIssue& a, const ConfigIssue& b) {
    return a.path == b.path ? static_cast<int>(a.code) < static_cast<int>(b.code) : a.path < b.path;
  });
  if (!issues.empty()) return ConfigResult<ResolvedHbfConfig>::failure(std::move(issues));

  std::map<std::string, SourceMetadata> sources;
  sources["product.family"] = {ConfigOrigin::Ocp, "OCP HBF v0.7.0 profile lock"};
  sources["product.profile"] = {ConfigOrigin::Ocp, "TASK1 profile selection"};
  for (const auto* path : {"geometry.host_channels", "geometry.core_dies", "geometry.dies_per_core",
                           "geometry.banks_per_die", "geometry.blocks_per_bank", "geometry.pages_per_block",
                           "geometry.page_bytes"}) sources[path] = {geometry_origin, source_detail};
  sources["geometry.page_count"] = {ConfigOrigin::Derived, "checked geometry product"};
  sources["geometry.capacity_bytes"] = {ConfigOrigin::Derived, "page_count * page_bytes"};
  for (const auto& field : component_fields) sources[field.first] = {ConfigOrigin::User, "explicit component selection"};
  sources["topology.channel_ownership"] = {ownership.synthetic ? ConfigOrigin::Synthetic : ConfigOrigin::User,
      ownership.synthetic ? "derived synthetic modulo ownership" : "explicit product ownership"};
  sources["model"] = {raw.scalars.count("model.source") ? ConfigOrigin::User : ConfigOrigin::Derived,
                       model.source};

  std::ostringstream canonical;
  canonical << "schema=openhbx.hbf.config.v1\nproduct.family=HBF\nproduct.profile=" << profile_name
            << "\ngeometry.host_channels=" << geometry.host_channels
            << "\ngeometry.core_dies=" << geometry.core_dies
            << "\ngeometry.dies_per_core=" << geometry.dies_per_core
            << "\ngeometry.banks_per_die=" << geometry.banks_per_die
            << "\ngeometry.blocks_per_bank=" << geometry.blocks_per_bank
            << "\ngeometry.pages_per_block=" << geometry.pages_per_block
            << "\ngeometry.page_bytes=" << geometry.page_bytes << " B"
            << "\ngeometry.page_count=" << geometry.page_count
            << "\ngeometry.capacity_bytes=" << geometry.capacity_bytes << " B"
            << "\ncomponents.host=" << components.host << "\ncomponents.address=" << components.address
            << "\ncomponents.controller=" << components.controller << "\ncomponents.interconnect=" << components.interconnect
            << "\ncomponents.media=" << components.media << "\ncomponents.ras=" << components.ras << '\n';
  canonical << "topology.channel_ownership=";
  for (std::size_t channel = 0; channel < ownership.banks_by_channel.size(); ++channel) {
    if (channel != 0) canonical << ';';
    for (std::size_t index = 0; index < ownership.banks_by_channel[channel].size(); ++index) {
      if (index != 0) canonical << ',';
      const auto& bank = ownership.banks_by_channel[channel][index];
      canonical << bank.core_die << '/' << bank.die << '/' << bank.bank;
    }
  }
  canonical << "\nmodel.source=" << model.source
      << "\nmodel.tck_picoseconds=" << model.tck_picoseconds
      << "\nmodel.completion_capacity=" << model.completion_capacity
      << "\nmodel.axi_interfaces=" << model.axi_interfaces
      << "\nmodel.host_queue_depth_per_interface=" << model.host_queue_depth_per_interface
      << "\nmodel.controller_pending_dlu=" << model.controller_pending_dlu
      << "\nmodel.controller_accumulation_timeout=" << model.controller_accumulation_timeout
      << "\nmodel.controller_queue_depth=" << model.controller_queue_depth
      << "\nmodel.controller_cache_buffers_per_bank=" << model.controller_cache_buffers_per_bank
      << "\nmodel.controller_ecc_credits=" << model.controller_ecc_credits
      << "\nmodel.controller_backend_timeout=" << model.controller_backend_timeout
      << "\nmodel.pal_max_inflight=" << model.pal_max_inflight
      << "\nmodel.pal_media_retry_budget=" << model.pal_media_retry_budget
      << "\nmodel.pal_return_retry_budget=" << model.pal_return_retry_budget
      << "\nmodel.pal_retry_delay_cycles=" << model.pal_retry_delay_cycles
      << "\nmodel.media_max_inflight=" << model.media_max_inflight
      << "\nmodel.reliability_seed=" << model.reliability_seed
      << "\nmodel.fabric_active_lanes=" << model.fabric_active_lanes
      << "\nmodel.fabric_spare_lanes=" << model.fabric_spare_lanes
      << "\nmodel.fabric_bits_per_lane_cycle=" << model.fabric_bits_per_lane_cycle
      << "\nmodel.fabric_efficiency_ppm=" << model.fabric_efficiency_ppm
      << "\nmodel.fabric_arbitration_cycles=" << model.fabric_arbitration_cycles
      << "\nmodel.fabric_propagation_cycles=" << model.fabric_propagation_cycles
      << "\nmodel.media_read_command_cycles=" << model.media_read_command_cycles
      << "\nmodel.media_read_sense_cycles=" << model.media_read_sense_cycles
      << "\nmodel.media_program_data_cycles=" << model.media_program_data_cycles
      << "\nmodel.media_program_array_cycles=" << model.media_program_array_cycles
      << "\nmodel.media_program_verify_cycles=" << model.media_program_verify_cycles
      << "\nmodel.media_erase_setup_cycles=" << model.media_erase_setup_cycles
      << "\nmodel.media_erase_array_cycles=" << model.media_erase_array_cycles
      << "\nmodel.media_erase_verify_cycles=" << model.media_erase_verify_cycles << '\n';
  const std::string bytes = canonical.str();
  return ConfigResult<ResolvedHbfConfig>::success(ResolvedHbfConfig(
      profile_name, geometry, std::move(components), std::move(ownership), std::move(model),
      std::move(sources), bytes, hash64(bytes)));
}
}  // namespace openhbx::config
