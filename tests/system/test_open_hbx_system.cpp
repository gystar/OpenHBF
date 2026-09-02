#include <cassert>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/hbf_product_composer.h"
#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/integration/request_bridge.h"
#include "openhbx/system/open_hbx_system.h"

using namespace openhbx;

namespace {
struct EmptyInstance final : config::ComponentInstance {};

config::ResolvedHbfConfig resolved_config(const std::string& page_bytes = "4096 B",
                                          std::uint8_t axi_interfaces = 1) {
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
  blocks_per_bank: 4
  pages_per_block: 4
  page_bytes: )" + page_bytes + R"(
components:
  host: OcpHbfTransactionLevel
  address: HbfR1R5
  controller: BaseDieFlash
  interconnect: HbfTsvBaseline
  media: NandFlash
  ras: NandRawRas
model:
  source: open-hbx:test-model-v1
  axi_interfaces: )" + std::to_string(axi_interfaces) + R"(
)";
  auto parsed = config::parse_hbf_yaml(yaml);
  assert(parsed);
  auto resolved = config::resolve_hbf_config(parsed.value());
  assert(resolved);
  return resolved.take_value();
}

std::unique_ptr<OpenHbxSystem> make_system(std::uint8_t axi_interfaces = 1) {
  auto built = OpenHbxSystem::compose(resolved_config("4096 B", axi_interfaces));
  assert(built);
  return std::move(built.system);
}

integration::BridgeRequest request(std::uint64_t address, std::uint32_t bytes,
                                   integration::BridgeRequestType type,
                                   std::uint8_t pattern,
                                   std::function<void(const SystemCompletion&)> completion) {
  integration::BridgeRequest result;
  result.address = static_cast<std::int64_t>(address);
  result.size_bytes = static_cast<std::int32_t>(bytes);
  result.source_id = static_cast<std::int32_t>(address / 64 + 1);
  result.ingress_id = 0;
  result.type = type;
  if (type == integration::BridgeRequestType::Write)
    result.payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(bytes, pattern));
  result.completion = std::move(completion);
  return result;
}

void submit_page(OpenHbxSystem& system, integration::RequestBridge& bridge,
                 std::uint64_t base, std::uint8_t pattern,
                 std::vector<SystemCompletion>& completions,
                 bool require_final_delivery = true) {
  for (std::uint64_t sector = 0; sector < 64; ++sector) {
    auto completion = [&](const SystemCompletion& value) {
      if (require_final_delivery)
        assert(system.current_phase() == EventPhase::FinalDelivery);
      completions.push_back(value);
    };
    assert(bridge.try_submit(request(base + sector * 64, 64,
                                     integration::BridgeRequestType::Write,
                                     pattern, completion)).code == AdmissionCode::Accepted);
  }
}

void tick_until(OpenHbxSystem& system, const std::function<bool()>& done,
                std::uint64_t budget = 200000) {
  for (std::uint64_t tick = 0; tick < budget && !done(); ++tick) system.tick();
  assert(done());
}

SystemCompletion read_page(OpenHbxSystem& system, integration::RequestBridge& bridge,
                           std::uint64_t address) {
  std::vector<SystemCompletion> completions;
  auto completion = [&](const SystemCompletion& value) {
    assert(system.current_phase() == EventPhase::FinalDelivery);
    completions.push_back(value);
  };
  assert(bridge.try_submit(request(address, 4096,
                                   integration::BridgeRequestType::Read, 0,
                                   completion)).code == AdmissionCode::Accepted);
  assert(completions.empty());
  tick_until(system, [&] { return completions.size() == 1; });
  assert(completions.size() == 1);
  return completions.front();
}

void assert_conservation(const OpenHbxSystem& system) {
  const auto state = system.snapshot().completions;
  assert(state.accepted == state.terminal + state.outstanding);
  assert(system.stats().accepted == system.stats().callbacks +
                                      system.stats().outstanding);
}

using FactorySpec = std::function<std::unique_ptr<config::ComponentInstance>(
    const std::string&, const std::string&)>;

config::ComponentRegistry component_registry(
    const config::ResolvedHbfConfig& resolved, const FactorySpec& make,
    std::uint64_t* registry_builds = nullptr, bool seal = true,
    bool omit_host = false) {
  config::ComponentRegistry registry;
  const auto& selected = resolved.components();
  const std::array<std::pair<const char*, const std::string*>, 6> components{{
      {"address", &selected.address}, {"media", &selected.media},
      {"interconnect", &selected.interconnect},
      {"controller", &selected.controller}, {"host", &selected.host},
      {"ras", &selected.ras}}};
  for (const auto& component : components) {
    if (omit_host && std::string(component.first) == "host") continue;
    const std::string kind = component.first;
    const std::string impl = *component.second;
    assert(registry.add(kind, impl,
        [kind, impl, make, registry_builds](const config::ResolvedHbfConfig&) {
          if (registry_builds) ++*registry_builds;
          return config::ConfigResult<std::unique_ptr<config::ComponentInstance>>::success(
              make(kind, impl));
        }).empty());
  }
  if (seal) registry.seal();
  return registry;
}

std::vector<std::string> dependencies(const std::string& kind) {
  if (kind == "address" || kind == "media") return {};
  if (kind == "interconnect") return {"address", "media"};
  if (kind == "controller") return {"address", "interconnect"};
  if (kind == "host") return {"address", "controller"};
  if (kind == "ras") return {"media"};
  return {"invalid"};
}

void test_typed_composer_atomicity() {
  auto resolved = resolved_config();

  // The production registry owns the only successful publication path.
  auto production = OpenHbxSystem::compose(resolved);
  assert(production);
  assert(production.system->resolved_config().canonical_hash() ==
         resolved.canonical_hash());

  std::vector<std::string> complete_order;
  auto complete = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, dependencies(kind),
            [&, kind](OpenHbxSystem&, std::string&) {
              complete_order.push_back(kind);
              return true;
            });
      });
  auto incomplete_owners = OpenHbxSystem::compose_with_registry(resolved, complete);
  assert(!incomplete_owners && !incomplete_owners.system);
  assert((complete_order == std::vector<std::string>{
      "address", "media", "interconnect", "controller", "host", "ras"}));

  std::vector<std::string> failure_order;
  auto failing = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, dependencies(kind),
            [&, kind](OpenHbxSystem&, std::string& error) {
              failure_order.push_back(kind);
              if (kind == "controller") {
                error = "injected controller factory failure";
                return false;
              }
              return true;
            });
      });
  auto failed = OpenHbxSystem::compose_with_registry(resolved, failing);
  assert(!failed && !failed.system);
  assert((failure_order == std::vector<std::string>{
      "address", "media", "interconnect", "controller"}));

  std::vector<std::string> empty_order;
  auto empty = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl)
          -> std::unique_ptr<config::ComponentInstance> {
        if (kind == "address") return std::make_unique<EmptyInstance>();
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, dependencies(kind),
            [&, kind](OpenHbxSystem&, std::string&) {
              empty_order.push_back(kind);
              return true;
            });
      });
  auto empty_failed = OpenHbxSystem::compose_with_registry(resolved, empty);
  assert(!empty_failed && !empty_failed.system && empty_order.empty());

  std::vector<std::string> duplicate_order;
  auto duplicate = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        const auto factory_kind = kind == "media" ? std::string("address") : kind;
        return std::make_unique<config::SystemComponentFactory>(
            factory_kind, impl, dependencies(factory_kind),
            [&, factory_kind](OpenHbxSystem&, std::string&) {
              duplicate_order.push_back(factory_kind);
              return true;
            });
      });
  auto duplicate_failed = OpenHbxSystem::compose_with_registry(resolved, duplicate);
  assert(!duplicate_failed && !duplicate_failed.system);
  assert((duplicate_order == std::vector<std::string>{"address"}));

  std::vector<std::string> dependency_order;
  auto wrong_dependency = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        auto deps = dependencies(kind);
        if (kind == "address") deps = {"media"};
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, std::move(deps),
            [&, kind](OpenHbxSystem&, std::string&) {
              dependency_order.push_back(kind);
              return true;
            });
      });
  auto dependency_failed =
      OpenHbxSystem::compose_with_registry(resolved, wrong_dependency);
  assert(!dependency_failed && !dependency_failed.system && dependency_order.empty());

  std::uint64_t unsealed_registry_builds = 0;
  std::vector<std::string> unsealed_factory_builds;
  auto unsealed = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, dependencies(kind),
            [&, kind](OpenHbxSystem&, std::string&) {
              unsealed_factory_builds.push_back(kind);
              return true;
            });
      }, &unsealed_registry_builds, false);
  auto unsealed_failed = OpenHbxSystem::compose_with_registry(resolved, unsealed);
  assert(!unsealed_failed && !unsealed_failed.system);
  assert(unsealed_registry_builds == 0 && unsealed_factory_builds.empty());

  std::uint64_t unknown_registry_builds = 0;
  std::vector<std::string> unknown_factory_builds;
  auto unknown = component_registry(resolved,
      [&](const std::string& kind, const std::string& impl) {
        return std::make_unique<config::SystemComponentFactory>(
            kind, impl, dependencies(kind),
            [&, kind](OpenHbxSystem&, std::string&) {
              unknown_factory_builds.push_back(kind);
              return true;
            });
      }, &unknown_registry_builds, true, true);
  auto unknown_failed = OpenHbxSystem::compose_with_registry(resolved, unknown);
  assert(!unknown_failed && !unknown_failed.system);
  assert(unknown_registry_builds == 4 && unknown_factory_builds.empty());
}

std::string run_digest() {
  auto system = make_system();
  integration::RequestBridge bridge(*system);
  std::vector<SystemCompletion> writes;
  submit_page(*system, bridge, 0, 0x5A, writes);
  assert(writes.empty());
  const auto reserved = system->debug_address_snapshot(0);
  assert(reserved);
  assert(reserved->sequence.mode == hbf::address::SequenceMode::Reserved);
  assert(reserved->sequence.outstanding);
  assert(reserved->sequence.expected_page == 0);
  assert(!reserved->page_valid);
  tick_until(*system, [&] { return writes.size() == 64; });
  const auto committed = system->debug_address_snapshot(0);
  assert(committed);
  assert(committed->sequence.mode == hbf::address::SequenceMode::Sequential);
  assert(!committed->sequence.outstanding);
  assert(committed->sequence.expected_page == 1);
  assert(committed->block_erase_count == 1);
  assert(committed->page_valid);
  assert(writes.size() == 64);
  const auto result = read_page(*system, bridge, 0);
  assert(result.command_status == 0 && result.data_valid && result.payload.size() == 4096);
  for (auto byte : result.payload.bytes()) assert(byte == 0x5A);
  assert_conservation(*system);
  const auto stats = system->stats();
  return std::to_string(result.completed_at.value()) + ":" +
         std::to_string(stats.ticks) + ":" +
         std::to_string(result.payload.bytes().front());
}

void test_final_delivery_throw_and_reentry() {
  auto system = make_system();
  integration::RequestBridge bridge(*system);
  std::uint64_t first_callbacks = 0;
  std::uint64_t reentrant_callbacks = 0;
  Cycle first_cycle;
  Cycle reentrant_cycle;

  auto first = request(0, 64, integration::BridgeRequestType::Read, 0,
      [&](const SystemCompletion&) {
        assert(system->current_phase() == EventPhase::FinalDelivery);
        ++first_callbacks;
        first_cycle = system->cycle();
        auto nested = request(64, 64, integration::BridgeRequestType::Read, 0,
            [&](const SystemCompletion&) {
              assert(system->current_phase() == EventPhase::FinalDelivery);
              ++reentrant_callbacks;
              reentrant_cycle = system->cycle();
            });
        assert(bridge.try_submit(std::move(nested)).code == AdmissionCode::Accepted);
      });
  assert(bridge.try_submit(std::move(first)).code == AdmissionCode::Accepted);
  assert(first_callbacks == 0 && reentrant_callbacks == 0);
  assert(system->snapshot().completions.outstanding == 1);
  tick_until(*system, [&] { return reentrant_callbacks == 1; });
  assert(first_callbacks == 1 && reentrant_callbacks == 1);
  assert(reentrant_cycle > first_cycle);
  assert_conservation(*system);

  auto throwing = request(128, 64, integration::BridgeRequestType::Read, 0,
      [&](const SystemCompletion&) {
        assert(system->current_phase() == EventPhase::FinalDelivery);
        throw std::runtime_error("sink");
      });
  assert(bridge.try_submit(std::move(throwing)).code == AdmissionCode::Accepted);
  tick_until(*system, [&] { return system->stats().callback_errors == 1; });
  assert(system->drain(20000).drained);
  assert(system->stats().callback_errors == 1);
  assert_conservation(*system);
}

void test_reentrant_staged_terminal_survives_reset() {
  auto system = make_system();
  integration::RequestBridge bridge(*system);
  std::vector<SystemCompletion> writes;
  submit_page(*system, bridge, 0, 0x6D, writes);
  tick_until(*system, [&] { return writes.size() == 64; });

  // Populate the Controller cache so both sector reads terminally stage during
  // admission, while their user callbacks remain deferred to FinalDelivery.
  const auto page = read_page(*system, bridge, 0);
  assert(page.command_status == 0 && page.data_valid);

  std::uint64_t outer_callbacks = 0;
  std::uint64_t nested_callbacks = 0;
  Cycle outer_cycle;
  Cycle nested_cycle;
  Generation nested_generation;
  EventQueueSnapshot before_reset_events;
  EventQueueSnapshot after_reset_request_events;
  CompletionSnapshot before_reset_completions;
  auto outer = request(0, 64, integration::BridgeRequestType::Read, 0,
      [&](const SystemCompletion& completion) {
        assert(system->current_phase() == EventPhase::FinalDelivery);
        assert(completion.command_status == 0 && completion.data_valid);
        ++outer_callbacks;
        outer_cycle = system->cycle();

        auto nested = request(64, 64, integration::BridgeRequestType::Read, 0,
            [&](const SystemCompletion& value) {
              assert(system->current_phase() == EventPhase::FinalDelivery);
              assert(value.command_status == 0 && value.data_valid);
              ++nested_callbacks;
              nested_cycle = system->cycle();
              nested_generation = value.generation;
            });
        assert(bridge.try_submit(std::move(nested)).code == AdmissionCode::Accepted);
        assert(nested_callbacks == 0);
        assert(system->snapshot().completions.outstanding == 1);
        const auto before_reset = system->snapshot();
        before_reset_events = before_reset.events;
        before_reset_completions = before_reset.completions;
        assert(system->reset());
        after_reset_request_events = system->snapshot().events;
        assert(after_reset_request_events.scheduled ==
               before_reset_events.scheduled + 1);
        assert(after_reset_request_events.queued == before_reset_events.queued + 1);
        assert(after_reset_request_events.dispatched == before_reset_events.dispatched);
        assert(after_reset_request_events.stale == before_reset_events.stale);
        assert(nested_callbacks == 0);
      });
  assert(bridge.try_submit(std::move(outer)).code == AdmissionCode::Accepted);
  assert(outer_callbacks == 0 && nested_callbacks == 0);

  system->tick();
  assert(outer_callbacks == 1 && nested_callbacks == 0);
  const auto old_generation = system->generation();
  system->tick();
  const auto after_delivery = system->snapshot();
  assert(system->generation().value() == old_generation.value() + 1);
  assert(nested_callbacks == 1);
  assert(nested_cycle > outer_cycle);
  // A Delivering obligation retains the terminal generation/result staged
  // before reset; only its delivery event is fenced into the new generation.
  assert(nested_generation == old_generation);
  // Reset rebases the existing nested FinalDelivery event. It schedules no
  // replacement or duplicate. The callback snapshot precedes dispatch_due's
  // accounting of the outer event, so the delta is outer + Reset + nested.
  assert(after_delivery.events.scheduled == after_reset_request_events.scheduled);
  assert(after_delivery.events.dispatched ==
         after_reset_request_events.dispatched + 3);
  assert(after_delivery.events.stale == after_reset_request_events.stale);
  assert(after_delivery.events.queued + 2 == after_reset_request_events.queued);
  assert(after_delivery.completions.terminal ==
         before_reset_completions.terminal + 1);
  assert(after_delivery.completions.outstanding == 0);
  assert_conservation(*system);
  assert(system->drain(20000).drained);
}

void test_async_reset_discards_uncommitted_program() {
  auto system = make_system();
  integration::RequestBridge bridge(*system);
  std::vector<SystemCompletion> writes;
  submit_page(*system, bridge, 0, 0xA5, writes);
  const auto old_generation = system->generation();
  const auto before_reset = system->snapshot();
  const auto reserved = system->debug_address_snapshot(0);
  assert(reserved && reserved->sequence.outstanding && !reserved->page_valid);

  assert(system->reset());
  assert(system->generation() == old_generation);
  assert(writes.empty());
  assert(system->snapshot().completions.outstanding == 64);
  assert(!system->reset());

  system->tick();
  assert(system->generation() == old_generation);
  assert(writes.empty());
  system->tick();
  assert(system->generation().value() == old_generation.value() + 1);
  assert(writes.size() == 64);
  for (const auto& completion : writes) {
    assert(completion.command_status == 0xE);
    assert(!completion.data_valid && completion.payload.empty());
  }
  const auto after_reset = system->debug_address_snapshot(0);
  assert(after_reset);
  assert(after_reset->sequence.mode == hbf::address::SequenceMode::Empty);
  assert(!after_reset->sequence.outstanding);
  assert(after_reset->sequence.expected_page == 0);
  assert(!after_reset->page_valid);
  assert(after_reset->block_erase_count == 0);
  assert(before_reset.completions.terminal == 0);
  assert_conservation(*system);

  const auto read = read_page(*system, bridge, 0);
  assert(read.command_status != 0);
  assert(!read.data_valid && read.payload.empty());
  assert_conservation(*system);
}

void test_duplicate_token_and_drain_resume() {
  auto system = make_system();
  std::uint64_t callbacks = 0;
  hbf::host::HostIngress ingress;
  ingress.generation = system->generation();
  ingress.channel = hbf::address::ChannelId(0);
  ingress.axi_interface = 0;
  ingress.axi_id = hbf::host::AxiId(1);
  ingress.packet_type = hbf::host::PacketType::FlashIo;
  ingress.operation = hbf::host::HostOperation::Read;
  ingress.size_bytes = 64;
  auto callback = [&](SystemCompletion) {
    assert(system->current_phase() == EventPhase::FinalDelivery);
    ++callbacks;
  };
  assert(system->try_submit({Token(77), ingress, callback}).code == AdmissionCode::Accepted);
  const auto before = system->snapshot();
  ingress.axi_id = hbf::host::AxiId(2);
  assert(system->try_submit({Token(77), ingress, callback}).code == AdmissionCode::Rejected);
  const auto after = system->snapshot();
  assert(after.completions.accepted == before.completions.accepted);
  assert(after.completions.outstanding == before.completions.outstanding);
  assert(!system->drain(1).drained);
  assert(system->drain(20000).drained && callbacks == 1);
  assert_conservation(*system);
}

void test_multi_axi_payload_isolation() {
  auto system = make_system(2);
  integration::RequestBridge bridge(*system);
  const auto axi_slice = system->resolved_config().geometry().capacity_bytes / 2;
  std::vector<SystemCompletion> first_writes;
  std::vector<SystemCompletion> second_writes;
  submit_page(*system, bridge, 0, 0x31, first_writes);
  tick_until(*system, [&] { return first_writes.size() == 64; });
  submit_page(*system, bridge, axi_slice, 0xC7, second_writes);
  tick_until(*system, [&] { return second_writes.size() == 64; });

  const auto first = read_page(*system, bridge, 0);
  const auto second = read_page(*system, bridge, axi_slice);
  assert(first.command_status == 0 && first.data_valid && first.payload.size() == 4096);
  assert(second.command_status == 0 && second.data_valid && second.payload.size() == 4096);
  for (auto byte : first.payload.bytes()) assert(byte == 0x31);
  for (auto byte : second.payload.bytes()) assert(byte == 0xC7);
  assert_conservation(*system);
}
}  // namespace

int main() {
  test_typed_composer_atomicity();
  const auto first = run_digest();
  assert(first == run_digest());
  assert(first == run_digest());
  test_final_delivery_throw_and_reentry();
  test_reentrant_staged_terminal_survives_reset();
  test_async_reset_discards_uncommitted_program();
  test_duplicate_token_and_drain_resume();
  test_multi_axi_payload_isolation();

  auto invalid = OpenHbxSystem::compose(resolved_config("8192 B"));
  assert(!invalid && invalid.error.find("4096 B") != std::string::npos);
  std::cout << "determinism_digest=" << first << " result=pass\n";
}
