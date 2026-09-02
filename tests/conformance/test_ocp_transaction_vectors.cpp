#include <cassert>
#include <cstdint>
#include <functional>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/integration/request_bridge.h"
#include "openhbx/system/open_hbx_system.h"

using namespace openhbx;

namespace {
config::ResolvedHbfConfig resolved_config_fixture() {
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
  pages_per_block: 2
  page_bytes: 4096 B
components:
  host: OcpHbfTransactionLevel
  address: HbfR1R5
  controller: BaseDieFlash
  interconnect: HbfTsvBaseline
  media: NandFlash
  ras: NandRawRas
model:
  source: open-hbx:s9-ocp-vector-v1
  axi_interfaces: 1
)";
  const auto parsed = config::parse_hbf_yaml(yaml);
  assert(parsed);
  auto resolved = config::resolve_hbf_config(parsed.value());
  assert(resolved);
  return resolved.take_value();
}

integration::BridgeRequest request(std::uint64_t address, std::uint32_t bytes,
                                   integration::BridgeRequestType type,
                                   std::uint64_t id,
                                   std::function<void(const SystemCompletion&)> sink) {
  integration::BridgeRequest result;
  result.address = static_cast<std::int64_t>(address);
  result.size_bytes = static_cast<std::int32_t>(bytes);
  result.source_id = static_cast<std::int32_t>(id);
  result.ingress_id = 0;
  result.type = type;
  if (type == integration::BridgeRequestType::Write)
    result.payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(bytes, 0x6d));
  result.completion = std::move(sink);
  return result;
}

void tick_until(OpenHbxSystem& system, const std::function<bool()>& done) {
  for (std::uint64_t cycle = 0; cycle < 200000 && !done(); ++cycle) system.tick();
  assert(done());
}
}

int main() {
  std::ifstream vectors(std::string(OPENHBX_SOURCE_DIR) +
                        "/tests/conformance/vectors/ocp_hbf_v0_7_vectors.json");
  assert(vectors.good());
  const std::string frozen((std::istreambuf_iterator<char>(vectors)),
                           std::istreambuf_iterator<char>());
  assert(frozen.find("OCP-5.3-64B") != std::string::npos);
  assert(frozen.find("OCP-5.4-DLU") != std::string::npos);
  assert(frozen.find("OCP-SPEED-GRADE-3") != std::string::npos);

  // Product Description speed grade 3 freezes both the raw link derivation
  // and the separately stated user-bandwidth target.
  constexpr std::uint64_t line_rate_gts = 32;
  constexpr std::uint64_t channel_bits = 64;
  constexpr std::uint64_t channels = 16;
  const auto raw_channel_gbps = line_rate_gts * channel_bits / 8;
  const auto raw_module_gbps = raw_channel_gbps * channels;
  constexpr std::uint64_t user_target_gbps = 3072;
  assert(raw_channel_gbps == 256 && raw_module_gbps == 4096 &&
         user_target_gbps == 3072);

  auto built = OpenHbxSystem::compose(resolved_config_fixture());
  assert(built);
  auto& system = *built.system;
  integration::RequestBridge bridge(system);
  std::vector<SystemCompletion> completions;
  auto sink = [&](const SystemCompletion& completion) {
    assert(system.current_phase() == EventPhase::FinalDelivery);
    completions.push_back(completion);
  };

  // OCP sections 5.3/5.4: transaction size is a 64 B multiple and a burst
  // cannot cross the 4 KiB DLU boundary. Rejection creates no obligation.
  const auto before = system.snapshot().completions;
  assert(bridge.try_submit(request(0, 63, integration::BridgeRequestType::Read,
                                   1, sink)).code == AdmissionCode::Rejected);
  assert(bridge.try_submit(request(4096 - 64, 128,
                                   integration::BridgeRequestType::Read,
                                   2, sink)).code == AdmissionCode::Rejected);
  const auto rejected = system.snapshot().completions;
  assert(rejected.accepted == before.accepted &&
         rejected.outstanding == before.outstanding && completions.empty());

  // OCP section 5.4: 64 sector writes form one 4 KiB DLU and are non-posted.
  for (std::uint64_t sector = 0; sector < 64; ++sector) {
    assert(bridge.try_submit(request(sector * 64, 64,
                                     integration::BridgeRequestType::Write,
                                     sector + 10, sink)).code == AdmissionCode::Accepted);
  }
  assert(completions.empty());
  tick_until(system, [&] { return completions.size() == 64; });
  for (const auto& completion : completions)
    assert(completion.command_status == 0 && !completion.data_valid);

  completions.clear();
  assert(bridge.try_submit(request(0, 4096, integration::BridgeRequestType::Read,
                                   100, sink)).code == AdmissionCode::Accepted);
  tick_until(system, [&] { return completions.size() == 1; });
  assert(completions.front().command_status == 0 && completions.front().data_valid &&
         completions.front().payload.size() == 4096);
  for (auto byte : completions.front().payload.bytes()) assert(byte == 0x6d);

  const auto final = system.snapshot().completions;
  assert(final.accepted == final.terminal + final.outstanding);
  assert(final.outstanding == 0);
}
