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
constexpr std::uint64_t kPageBytes = 4096;
constexpr std::uint64_t kPages = 16;

config::ResolvedHbfConfig resolved_config() {
  const std::string yaml = R"(
product:
  family: HBF
  profile: OCP_HBF_0_7
geometry:
  source: synthetic
  host_channels: 1
  core_dies: 1
  dies_per_core: 1
  banks_per_die: 1
  blocks_per_bank: 2
  pages_per_block: 16
  page_bytes: 4096 B
components:
  host: OcpHbfTransactionLevel
  address: HbfR1R5
  controller: BaseDieFlash
  interconnect: HbfTsvBaseline
  media: NandFlash
  ras: NandRawRas
model:
  source: open-hbx:synthetic-bandwidth-v1
  tck_picoseconds: 1000
  axi_interfaces: 1
)";
  const auto parsed = config::parse_hbf_yaml(yaml);
  assert(parsed);
  auto resolved = config::resolve_hbf_config(parsed.value());
  assert(resolved);
  return resolved.take_value();
}

integration::BridgeRequest make_request(
    std::uint64_t address, integration::BridgeRequestType type,
    std::function<void(const SystemCompletion&)> completion) {
  integration::BridgeRequest request;
  request.address = static_cast<std::int64_t>(address);
  request.size_bytes = type == integration::BridgeRequestType::Write ? 64 : 4096;
  request.source_id = static_cast<std::int32_t>(address / 64 + 1);
  request.ingress_id = 0;
  request.type = type;
  if (type == integration::BridgeRequestType::Write)
    request.payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x5a));
  request.completion = std::move(completion);
  return request;
}

void tick_until(OpenHbxSystem& system, const std::function<bool()>& done,
                std::uint64_t budget = 200000) {
  for (std::uint64_t i = 0; i < budget && !done(); ++i) system.tick();
  assert(done());
}

std::uint64_t percentile(std::vector<std::uint64_t> samples, std::uint64_t percent) {
  assert(!samples.empty() && percent <= 100);
  std::sort(samples.begin(), samples.end());
  const std::size_t index = static_cast<std::size_t>(
      ((samples.size() - 1) * percent + 99) / 100);
  return samples[index];
}
}

int main() {
  const auto config = resolved_config();
  auto built = OpenHbxSystem::compose(config);
  assert(built);
  auto& system = *built.system;
  integration::RequestBridge bridge(system);

  std::uint64_t write_completions = 0;
  for (std::uint64_t page = 0; page < kPages; ++page) {
    const std::uint64_t page_completion_target = write_completions + 64;
    for (std::uint64_t sector = 0; sector < 64; ++sector) {
      const auto address = page * kPageBytes + sector * 64;
      auto request = make_request(address, integration::BridgeRequestType::Write,
          [&](const SystemCompletion& completion) {
            assert(completion.command_status == 0 && !completion.data_valid);
            ++write_completions;
          });
      for (;;) {
        const auto admission = bridge.try_submit(request);
        if (admission.code == AdmissionCode::Accepted) break;
        assert(admission.code == AdmissionCode::Busy);
        system.tick();
      }
    }
    // NAND page programs are sequential. Commit each page before reserving the
    // next page so fixture setup follows the same production rule as clients.
    tick_until(system, [&] { return write_completions == page_completion_target; });
  }
  assert(write_completions == kPages * 64);

  assert(system.reset());
  const auto next_generation = system.generation().value() + 1;
  tick_until(system, [&] { return system.generation().value() == next_generation; });

  std::uint64_t warmup = 0;
  auto warmup_request = make_request(0, integration::BridgeRequestType::Read,
      [&](const SystemCompletion& completion) {
        assert(completion.command_status == 0 && completion.data_valid &&
               completion.payload.size() == kPageBytes);
        ++warmup;
      });
  assert(bridge.try_submit(warmup_request).code == AdmissionCode::Accepted);
  tick_until(system, [&] { return warmup == 1; });

  const auto before = system.stats();
  const std::uint64_t measurement_start = system.cycle().value();
  std::uint64_t reads = 0;
  std::vector<std::uint64_t> latencies;
  latencies.reserve(kPages - 1);
  for (std::uint64_t page = 1; page < kPages; ++page) {
    const std::uint64_t submitted_at = system.cycle().value();
    auto read = make_request(page * kPageBytes, integration::BridgeRequestType::Read,
        [&, submitted_at](const SystemCompletion& completion) {
          assert(completion.command_status == 0 && completion.data_valid &&
                 completion.payload.size() == kPageBytes);
          for (auto byte : completion.payload.bytes()) assert(byte == 0x5a);
          assert(completion.completed_at.value() >= submitted_at);
          latencies.push_back(completion.completed_at.value() - submitted_at);
          ++reads;
        });
    for (;;) {
      const auto admission = bridge.try_submit(read);
      if (admission.code == AdmissionCode::Accepted) break;
      assert(admission.code == AdmissionCode::Busy);
      system.tick();
    }
  }
  tick_until(system, [&] { return reads == kPages - 1; });
  const std::uint64_t measurement_end = system.cycle().value();
  const auto after = system.stats();

  const std::uint64_t bytes = (kPages - 1) * kPageBytes;
  const std::uint64_t cycles = measurement_end - measurement_start;
  assert(cycles != 0);
  assert(after.read_completed_requests - before.read_completed_requests == kPages - 1);
  assert(after.read_completed_bytes - before.read_completed_bytes == bytes);
  assert(after.read_accepted_bytes - before.read_accepted_bytes == bytes);
  assert(after.failed_requests == before.failed_requests);
  assert(after.outstanding == 0);

  const auto& model = config.system_model();
  const std::uint64_t raw_bits_per_cycle =
      model.fabric_active_lanes * model.fabric_bits_per_lane_cycle;
  const std::uint64_t fabric_bits_per_cycle =
      raw_bits_per_cycle * model.fabric_efficiency_ppm / 1000000;
  assert(fabric_bits_per_cycle != 0);
  const double measured_bytes_per_cycle =
      static_cast<double>(bytes) / static_cast<double>(cycles);
  const double elapsed_nanoseconds =
      static_cast<double>(cycles) * static_cast<double>(model.tck_picoseconds) /
      1000.0;
  const double measured_gigabytes_per_second =
      measured_bytes_per_cycle * 1000.0 /
      static_cast<double>(model.tck_picoseconds);
  const std::uint64_t command_bits = 512;
  const auto transfer_cycles = [fabric_bits_per_cycle](std::uint64_t bits) {
    return (bits + fabric_bits_per_cycle - 1) / fabric_bits_per_cycle;
  };
  const std::uint64_t forward_cycles = model.fabric_arbitration_cycles +
      transfer_cycles(command_bits) + model.fabric_propagation_cycles;
  const std::uint64_t return_cycles = model.fabric_arbitration_cycles +
      transfer_cycles(kPageBytes * 8 + command_bits) +
      model.fabric_propagation_cycles;
  const double fabric_ceiling_bytes_per_cycle = static_cast<double>(kPageBytes) /
      static_cast<double>(std::max(forward_cycles, return_cycles));
  assert(measured_bytes_per_cycle <= fabric_ceiling_bytes_per_cycle);

  const std::uint64_t p50 = percentile(latencies, 50);
  const std::uint64_t p95 = percentile(latencies, 95);
  const auto minmax = std::minmax_element(latencies.begin(), latencies.end());
  std::ostringstream json;
  json << std::fixed << std::setprecision(6)
       << "{\n"
       << "  \"experiment_id\": \"EXP-PERF-HOST-FLASH-READ\",\n"
       << "  \"candidate_evidence_level\": \"E4-model-resource\",\n"
       << "  \"profile_source\": \"" << model.source << "\",\n"
       << "  \"config_hash\": \"" << config.canonical_hash() << "\",\n"
       << "  \"production_path\": \"Host->Address->Controller->PAL/Fabric->NAND->Host\",\n"
       << "  \"warmup_requests\": 1,\n"
       << "  \"measurement_requests\": " << reads << ",\n"
       << "  \"measurement_bytes\": " << bytes << ",\n"
       << "  \"measurement_cycles\": " << cycles << ",\n"
       << "  \"tck_picoseconds\": " << model.tck_picoseconds << ",\n"
       << "  \"clock_assumption\": \"synthetic OpenHBX baseline; 1 ns/cycle\",\n"
       << "  \"elapsed_nanoseconds\": " << elapsed_nanoseconds << ",\n"
       << "  \"bytes_per_cycle\": " << measured_bytes_per_cycle << ",\n"
       << "  \"gigabytes_per_second\": "
       << measured_gigabytes_per_second << ",\n"
       << "  \"fabric_ceiling_bytes_per_cycle\": "
       << fabric_ceiling_bytes_per_cycle << ",\n"
       << "  \"fabric_ceiling_formula\": \"4096/max(forward_command_cycles,return_data_cycles)\",\n"
       << "  \"latency_cycles\": {\"min\": " << *minmax.first
       << ", \"p50\": " << p50 << ", \"p95\": " << p95
       << ", \"max\": " << *minmax.second << "},\n"
       << "  \"accepted_terminal_conservation\": true,\n"
       << "  \"claim_limit\": \"GB/s uses synthetic 1 ns tCK; not vendor/OCP guaranteed bandwidth\"\n"
       << "}\n";
  std::cout << json.str();
  if (const char* path = std::getenv("OPENHBX_BANDWIDTH_LOG")) {
    const std::filesystem::path output(path);
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output);
    assert(stream);
    stream << json.str();
  }
}
