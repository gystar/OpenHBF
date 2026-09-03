#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/integration/request_bridge.h"
#include "openhbx/system/open_hbx_system.h"

using namespace openhbx;

namespace {
constexpr std::uint64_t kFixtureProgramArrayCycles = 20;

void print_phase(const char* name, const std::string& detail) {
  std::cout << "[OpenHBX] " << std::left << std::setw(9) << name << detail << '\n'
            << std::flush;
}

config::ResolvedHbfConfig resolved_config() {
  std::ifstream input(OPENHBX_PRODUCT_CONFIG_PATH);
  assert(input);
  std::ostringstream yaml;
  yaml << input.rdbuf();
  auto parsed = config::parse_hbf_yaml(yaml.str());
  assert(parsed);
  auto raw = parsed.take_value();
  // Page programming is fixture setup and is outside the read measurement window.
  raw.scalars["model.media_program_array_cycles"] =
      std::to_string(kFixtureProgramArrayCycles);
  auto resolved = config::resolve_hbf_config(raw);
  assert(resolved);
  return resolved.take_value();
}

integration::BridgeRequest request(
    std::uint64_t address, integration::BridgeRequestType type,
    std::uint64_t page_bytes,
    std::function<void(const SystemCompletion&)> completion) {
  integration::BridgeRequest value;
  value.address = static_cast<std::int64_t>(address);
  value.size_bytes = static_cast<std::int32_t>(
      type == integration::BridgeRequestType::Write ? 64 : page_bytes);
  value.source_id = static_cast<std::int32_t>(address / 64 + 1);
  value.ingress_id = 0;
  value.type = type;
  if (type == integration::BridgeRequestType::Write)
    value.payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0xa5));
  value.completion = std::move(completion);
  return value;
}

void submit_retry(OpenHbxSystem& system, integration::RequestBridge& bridge,
                  integration::BridgeRequest request) {
  for (;;) {
    const auto result = bridge.try_submit(request);
    if (result.code == AdmissionCode::Accepted) return;
    assert(result.code == AdmissionCode::Busy);
    system.tick();
  }
}

void tick_until(OpenHbxSystem& system, const std::function<bool()>& done,
                std::uint64_t budget = 500000) {
  for (std::uint64_t cycle = 0; cycle < budget && !done(); ++cycle) system.tick();
  assert(done());
}

std::uint64_t page_address(const config::ResolvedHbfConfig& config,
                           std::uint64_t channel, std::uint64_t owned_bank) {
  const std::uint64_t channel_capacity =
      config.geometry().capacity_bytes / config.geometry().host_channels;
  return channel * channel_capacity + owned_bank * config.geometry().page_bytes;
}

std::uint64_t percentile(std::vector<std::uint64_t> values, std::uint64_t percent) {
  std::sort(values.begin(), values.end());
  return values[((values.size() - 1) * percent + 99) / 100];
}
}  // namespace

int main() {
  const auto config = resolved_config();
  std::cout << "\n"
            << "OpenHBX HBF Maximum Read Bandwidth\n"
            << "==================================\n";
  print_phase("CONFIG", "loaded configs/products/ocp_hbf_0_7.yaml");
  auto built = OpenHbxSystem::compose(config);
  assert(built);
  auto& system = *built.system;
  integration::RequestBridge bridge(system);
  const std::uint64_t channels = config.geometry().host_channels;
  const std::uint64_t axi_per_channel = config.system_model().axi_interfaces;
  assert(config.ownership().banks_by_channel.size() == channels);
  const std::uint64_t banks_per_channel =
      config.ownership().banks_by_channel.front().size();
  const std::uint64_t page_bytes = config.geometry().page_bytes;
  const std::uint64_t pages = channels * banks_per_channel;
  assert(page_bytes % 64 == 0);
  const std::uint64_t sectors_per_page = page_bytes / 64;

  print_phase("SETUP", "programming " + std::to_string(pages) +
      " pages across " + std::to_string(channels) + " channels");
  std::uint64_t writes = 0;
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    for (std::uint64_t bank = 0; bank < banks_per_channel; ++bank) {
      const auto base = page_address(config, channel, bank);
      for (std::uint64_t sector = 0; sector < sectors_per_page; ++sector) {
        submit_retry(system, bridge,
            request(base + sector * 64, integration::BridgeRequestType::Write,
                page_bytes,
                [&](const SystemCompletion& completion) {
                  assert(completion.command_status == 0 && !completion.data_valid);
                  ++writes;
                }));
      }
    }
  }
  tick_until(system, [&] { return writes == pages * sectors_per_page; });

  print_phase("RESET", "clearing volatile controller state before measurement");
  assert(system.reset());
  const auto generation = system.generation().value() + 1;
  tick_until(system, [&] { return system.generation().value() == generation; });

  print_phase("MEASURE", "issuing one 4 KiB read to every channel-owned bank");
  system.enable_periodic_bandwidth_log(2000);
  const auto before = system.stats();
  const std::uint64_t start = system.cycle().value();
  std::uint64_t reads = 0;
  std::vector<std::uint64_t> latencies;
  latencies.reserve(pages);
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    for (std::uint64_t bank = 0; bank < banks_per_channel; ++bank) {
      const auto address = page_address(config, channel, bank);
      const auto submitted = system.cycle().value();
      submit_retry(system, bridge,
          request(address, integration::BridgeRequestType::Read, page_bytes,
              [&, submitted](const SystemCompletion& completion) {
                assert(completion.command_status == 0 && completion.data_valid &&
                       completion.payload.size() == page_bytes);
                for (const auto byte : completion.payload.bytes()) assert(byte == 0xa5);
                latencies.push_back(completion.completed_at.value() - submitted);
                ++reads;
              }));
    }
  }
  tick_until(system, [&] { return reads == pages; });
  const std::uint64_t end = system.cycle().value();
  const auto after = system.stats();

  const std::uint64_t bytes = pages * page_bytes;
  const std::uint64_t cycles = end - start;
  assert(cycles != 0 && after.outstanding == 0);
  assert(after.read_completed_requests - before.read_completed_requests == pages);
  assert(after.read_completed_bytes - before.read_completed_bytes == bytes);
  assert(after.failed_requests == before.failed_requests);

  const auto& model = config.system_model();
  const double bytes_per_cycle = static_cast<double>(bytes) / cycles;
  const double gigabytes_per_second =
      bytes_per_cycle * 1000.0 / model.tck_picoseconds;
  const double per_channel_raw_gigabytes_per_second =
      static_cast<double>(model.fabric_active_lanes) *
      model.fabric_bits_per_lane_cycle * model.fabric_efficiency_ppm /
      1000000.0 / 8.0 * 1000.0 / model.tck_picoseconds;
  const double aggregate_raw_ceiling =
      per_channel_raw_gigabytes_per_second * channels;
  const std::uint64_t media_read_service_cycles =
      model.media_read_command_cycles + model.media_read_sense_cycles;
  assert(media_read_service_cycles != 0);
  const double aggregate_media_ceiling =
      static_cast<double>(pages * page_bytes) / media_read_service_cycles *
      1000.0 / model.tck_picoseconds;
  const double effective_ceiling =
      std::min(aggregate_raw_ceiling, aggregate_media_ceiling);
  constexpr double kOcpUserTargetGigabytesPerSecond = 3072.0;
  const double raw_utilization = gigabytes_per_second / aggregate_raw_ceiling;
  const double effective_utilization = gigabytes_per_second / effective_ceiling;
  const double ocp_target_ratio =
      gigabytes_per_second / kOcpUserTargetGigabytesPerSecond;
  assert(gigabytes_per_second <= aggregate_raw_ceiling);
  assert(gigabytes_per_second <= aggregate_media_ceiling);

  const std::uint64_t latency_min =
      *std::min_element(latencies.begin(), latencies.end());
  const std::uint64_t latency_p50 = percentile(latencies, 50);
  const std::uint64_t latency_p95 = percentile(latencies, 95);
  const std::uint64_t latency_max =
      *std::max_element(latencies.begin(), latencies.end());

  print_phase("RESULT", "all requests completed and all performance oracles passed");
  std::cout << std::fixed << std::setprecision(3)
            << "\nexperiment:\n"
            << "  id:                         EXP-PERF-HBF-MAX-READ\n"
            << "  evidence:                   E4-model-resource candidate\n"
            << "  config_hash:                " << config.canonical_hash() << "\n"
            << "\ntopology:\n"
            << "  host_channels:              " << channels << "\n"
            << "  axi_interfaces_per_channel: " << axi_per_channel << "\n"
            << "  banks_per_channel:          " << banks_per_channel << "\n"
            << "  page_size:                  " << page_bytes << " B\n"
            << "\nlink_model:\n"
            << "  clock_period:               " << model.tck_picoseconds << " ps\n"
            << "  active_lanes_per_channel:   " << model.fabric_active_lanes << "\n"
            << "  bits_per_lane_per_cycle:    " << model.fabric_bits_per_lane_cycle << "\n"
            << "  modeled_link_efficiency:    "
            << static_cast<double>(model.fabric_efficiency_ppm) / 10000.0 << " %\n"
            << "  ocp_user_target_role:       report-only reference\n"
            << "  fixture_program_cycles:     " << kFixtureProgramArrayCycles
            << " (setup-only override)\n"
            << "\nmeasurement:\n"
            << "  requests:                   " << reads << " reads\n"
            << "  transferred:                "
            << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MiB\n"
            << "  window:                     " << cycles << " cycles ("
            << static_cast<double>(cycles) * model.tck_picoseconds / 1000.0
            << " ns)\n"
            << "  throughput:                 " << gigabytes_per_second
            << " GB/s (" << gigabytes_per_second / 1000.0 << " TB/s)\n"
            << "  raw_link_ceiling:           " << aggregate_raw_ceiling
            << " GB/s (" << aggregate_raw_ceiling / 1000.0 << " TB/s)\n"
            << "  raw_link_utilization:       " << raw_utilization * 100.0 << " %\n"
            << "  media_parallel_ceiling:     " << aggregate_media_ceiling
            << " GB/s\n"
            << "  effective_ceiling:          " << effective_ceiling << " GB/s\n"
            << "  effective_utilization:      " << effective_utilization * 100.0 << " %\n"
            << "  ocp_user_target:            " << kOcpUserTargetGigabytesPerSecond
            << " GB/s (" << kOcpUserTargetGigabytesPerSecond / 1000.0 << " TB/s)\n"
            << "  ocp_target_achievement:     " << ocp_target_ratio * 100.0 << " %\n"
            << "\nlatency_cycles:\n"
            << "  min:                        " << latency_min << "\n"
            << "  p50:                        " << latency_p50 << "\n"
            << "  p95:                        " << latency_p95 << "\n"
            << "  max:                        " << latency_max << "\n"
            << "\nchecks:\n"
            << "  completed_requests:         PASS (" << reads << '/' << pages << ")\n"
            << "  completed_bytes:            PASS (" << bytes << " B)\n"
            << "  payload_integrity:          PASS\n"
            << "  outstanding_after_drain:    PASS (0)\n"
            << "  failures:                   PASS (0)\n"
            << "  observed_le_raw_ceiling:    PASS\n"
            << "  observed_le_media_ceiling:  PASS\n"
            << "\nresult: PASS\n"
            << "note: synthetic transaction-level maximum; not a silicon guarantee\n";

  std::ostringstream json;
  json << std::fixed << std::setprecision(6)
       << "{\n"
       << "  \"experiment_id\": \"EXP-PERF-HBF-MAX-READ\",\n"
       << "  \"candidate_evidence_level\": \"E4-model-resource\",\n"
       << "  \"config_hash\": \"" << config.canonical_hash() << "\",\n"
       << "  \"host_channels\": " << channels << ",\n"
       << "  \"axi_interfaces_per_channel\": " << axi_per_channel << ",\n"
       << "  \"banks_per_channel\": " << banks_per_channel << ",\n"
       << "  \"measurement_requests\": " << reads << ",\n"
       << "  \"measurement_bytes\": " << bytes << ",\n"
       << "  \"measurement_cycles\": " << cycles << ",\n"
       << "  \"tck_picoseconds\": " << model.tck_picoseconds << ",\n"
       << "  \"fixture_program_array_cycles\": "
       << kFixtureProgramArrayCycles << ",\n"
       << "  \"bytes_per_cycle\": " << bytes_per_cycle << ",\n"
       << "  \"gigabytes_per_second\": " << gigabytes_per_second << ",\n"
       << "  \"per_channel_raw_ceiling_gigabytes_per_second\": "
       << per_channel_raw_gigabytes_per_second << ",\n"
       << "  \"aggregate_raw_ceiling_gigabytes_per_second\": "
       << aggregate_raw_ceiling << ",\n"
       << "  \"raw_ceiling_utilization\": " << raw_utilization << ",\n"
       << "  \"aggregate_media_ceiling_gigabytes_per_second\": "
       << aggregate_media_ceiling << ",\n"
       << "  \"effective_ceiling_gigabytes_per_second\": "
       << effective_ceiling << ",\n"
       << "  \"effective_ceiling_utilization\": "
       << effective_utilization << ",\n"
       << "  \"ocp_user_target_gigabytes_per_second\": "
       << kOcpUserTargetGigabytesPerSecond << ",\n"
       << "  \"ocp_user_target_ratio\": " << ocp_target_ratio << ",\n"
       << "  \"latency_cycles\": {\"min\": " << latency_min
       << ", \"p50\": " << latency_p50
       << ", \"p95\": " << latency_p95
       << ", \"max\": " << latency_max
       << "},\n"
       << "  \"claim_limit\": \"raw link is not pre-discounted; OCP user bandwidth is report-only target reference\"\n"
       << "}\n";
  if (const char* path = std::getenv("OPENHBX_MAX_BANDWIDTH_LOG")) {
    const std::filesystem::path output(path);
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output);
    assert(stream);
    stream << json.str();
    std::cout << "artifact: " << output.string() << '\n';
  }
}
