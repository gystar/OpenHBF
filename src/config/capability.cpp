#include "openhbx/config/capability.h"

#include <map>

namespace openhbx::config {
std::vector<ConfigIssue> validate_capability_closure(const ResolvedHbfConfig& config,
    const std::vector<ComponentDescriptor>& descriptors) {
  std::vector<ConfigIssue> issues;
  std::map<CapabilityId, std::size_t> providers;
  const auto& selected = config.components();
  const std::map<std::string, std::string> selections{{"host", selected.host},
      {"address", selected.address}, {"controller", selected.controller},
      {"interconnect", selected.interconnect}, {"media", selected.media},
      {"ras", selected.ras}};
  for (const auto& descriptor : descriptors) {
    const auto selected_impl = selections.find(descriptor.kind);
    if (selected_impl == selections.end() || selected_impl->second != descriptor.impl) continue;
    for (const auto capability : descriptor.provides) ++providers[capability];
  }
  const std::set<CapabilityId> required_product{CapabilityId::HbfHost,
      CapabilityId::R1R5Address, CapabilityId::SequentialProgram,
      CapabilityId::StackVertical, CapabilityId::NandProgramErase,
      CapabilityId::AsyncCompletion};
  for (const auto capability : required_product)
    if (providers[capability] == 0)
      issues.push_back({ConfigErrorCode::CapabilityMismatch, "components",
          "missing required capability " + std::to_string(static_cast<int>(capability)), 0});
  for (const auto& descriptor : descriptors) {
    const auto selected_impl = selections.find(descriptor.kind);
    if (selected_impl == selections.end() || selected_impl->second != descriptor.impl) continue;
    for (const auto requirement : descriptor.requirements)
      if (providers[requirement] == 0)
        issues.push_back({ConfigErrorCode::CapabilityMismatch,
            "components." + descriptor.kind,
            descriptor.impl + " has an unsatisfied capability requirement", 0});
  }
  if (config.profile() != "OCP_HBF_0_7")
    issues.push_back({ConfigErrorCode::UnsupportedProfile, "product.profile",
        "capability closure is defined only for OCP_HBF_0_7", 0});
  return issues;
}
}  // namespace openhbx::config
