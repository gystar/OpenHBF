#pragma once

#include <set>
#include <string>
#include <vector>

#include "openhbx/config/config_types.h"
#include "openhbx/config/resolved_hbf_config.h"

namespace openhbx::config {

enum class CapabilityId { HbfHost, R1R5Address, SequentialProgram, StackVertical, NandProgramErase, AsyncCompletion };
struct ComponentDescriptor {
  std::string kind;
  std::string impl;
  std::set<CapabilityId> provides;
  std::set<CapabilityId> requirements;
};

std::vector<ConfigIssue> validate_capability_closure(
    const ResolvedHbfConfig& config,
    const std::vector<ComponentDescriptor>& descriptors);

}  // namespace openhbx::config
