#include <cassert>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "openhbx/config/capability.h"
#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/hbf_product_composer.h"
#include "openhbx/config/resolved_hbf_config.h"

using namespace openhbx::config;

namespace {
struct Instance : ComponentInstance {
  Instance(int* live, int* destroyed) : live(live), destroyed(destroyed) { ++*live; }
  ~Instance() override { --*live; ++*destroyed; }
  int* live;
  int* destroyed;
};
struct EmptyInstance : ComponentInstance {};

ResolvedHbfConfig config() {
  RawConfigTree raw;
  raw.scalars = {{"product.profile", "OCP_HBF_0_7"}, {"geometry.source", "synthetic"},
      {"geometry.host_channels", "1"}, {"geometry.core_dies", "1"},
      {"geometry.dies_per_core", "1"}, {"geometry.banks_per_die", "1"},
      {"geometry.blocks_per_bank", "1"}, {"geometry.pages_per_block", "1"},
      {"geometry.page_bytes", "4096 B"}, {"components.host", "host"},
      {"components.address", "address"}, {"components.controller", "controller"},
      {"components.interconnect", "interconnect"}, {"components.media", "media"},
      {"components.ras", "ras"}};
  auto result = resolve_hbf_config(raw);
  assert(result);
  return result.take_value();
}
}

int main() {
  auto cfg = config();
  std::vector<ComponentDescriptor> descriptors{
      {"host", "host", {CapabilityId::HbfHost, CapabilityId::AsyncCompletion}, {}},
      {"address", "address", {CapabilityId::R1R5Address}, {}},
      {"controller", "controller", {CapabilityId::SequentialProgram}, {CapabilityId::NandProgramErase}},
      {"interconnect", "interconnect", {CapabilityId::StackVertical}, {}},
      {"media", "media", {CapabilityId::NandProgramErase}, {}}};
  assert(validate_capability_closure(cfg, descriptors).empty());
  descriptors.back().provides.clear();
  assert(!validate_capability_closure(cfg, descriptors).empty());

  ComponentRegistry duplicate;
  auto builder = [](const ResolvedHbfConfig&) {
    return ConfigResult<std::unique_ptr<ComponentInstance>>::success(
        std::make_unique<EmptyInstance>());
  };
  assert(duplicate.add("host", "host", builder).empty());
  assert(!duplicate.add("host", "host", builder).empty());

  int unsealed_builds = 0;
  ComponentRegistry unsealed;
  assert(unsealed.add("media", "media", [&unsealed_builds](const ResolvedHbfConfig&) {
    ++unsealed_builds;
    return ConfigResult<std::unique_ptr<ComponentInstance>>::success(
        std::make_unique<EmptyInstance>());
  }).empty());
  auto unsealed_result = compose_hbf_product(cfg, unsealed);
  assert(!unsealed_result && unsealed_builds == 0);

  int unknown_builds = 0;
  ComponentRegistry unknown;
  for (const auto& pair : std::vector<std::pair<std::string, std::string>>{{"media", "media"},
           {"interconnect", "interconnect"}, {"address", "address"},
           {"controller", "controller"}, {"host", "different-host"}, {"ras", "ras"}}) {
    assert(unknown.add(pair.first, pair.second,
                       [&unknown_builds](const ResolvedHbfConfig&) {
      ++unknown_builds;
      return ConfigResult<std::unique_ptr<ComponentInstance>>::success(
          std::make_unique<EmptyInstance>());
    }).empty());
  }
  unknown.seal();
  auto unknown_result = compose_hbf_product(cfg, unknown);
  assert(!unknown_result && unknown_builds == 4);

  int live = 0;
  int destroyed = 0;
  ComponentRegistry registry;
  for (const auto& pair : std::vector<std::pair<std::string, std::string>>{{"media", "media"},
           {"interconnect", "interconnect"}, {"address", "address"},
           {"controller", "controller"}, {"host", "host"}, {"ras", "ras"}}) {
    assert(registry.add(pair.first, pair.second, [&live, &destroyed](const ResolvedHbfConfig&) {
      return ConfigResult<std::unique_ptr<ComponentInstance>>::success(
          std::make_unique<Instance>(&live, &destroyed));
    }).empty());
  }
  registry.seal();
  assert(!registry.add("x", "x", builder).empty());
  auto assembly = compose_hbf_product(cfg, registry);
  assert(assembly && assembly.value()->size() == 6 && live == 6);
  assembly.take_value().reset();
  assert(live == 0 && destroyed == 6);

  ComponentRegistry failing;
  assert(failing.add("address", "address", [&live, &destroyed](const ResolvedHbfConfig&) {
    return ConfigResult<std::unique_ptr<ComponentInstance>>::success(
        std::make_unique<Instance>(&live, &destroyed));
  }).empty());
  assert(failing.add("media", "media", [&live, &destroyed](const ResolvedHbfConfig&) {
    return ConfigResult<std::unique_ptr<ComponentInstance>>::success(std::make_unique<Instance>(&live, &destroyed));
  }).empty());
  assert(failing.add("interconnect", "interconnect", [](const ResolvedHbfConfig&) {
    return ConfigResult<std::unique_ptr<ComponentInstance>>::failure({{
        ConfigErrorCode::BuildFailed, "components.interconnect", "injected failure", 0}});
  }).empty());
  failing.seal();
  auto failed = compose_hbf_product(cfg, failing);
  assert(!failed && live == 0 && destroyed == 8);
}
