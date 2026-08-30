#include "openhbx/config/component_registry.h"

namespace openhbx::config {
std::vector<ConfigIssue> ComponentRegistry::add(std::string kind, std::string impl,
                                                ComponentBuilder builder) {
  if (sealed_) return {{ConfigErrorCode::RegistrySealed, "components." + kind, "registry is sealed", 0}};
  if (!builder) return {{ConfigErrorCode::BuildFailed, "components." + kind, "builder must be callable", 0}};
  const auto inserted = builders_.emplace(std::make_pair(std::move(kind), std::move(impl)), std::move(builder));
  if (!inserted.second)
    return {{ConfigErrorCode::DuplicateRegistration,
        "components." + inserted.first->first.first, "kind + impl is already registered", 0}};
  return {};
}
const ComponentBuilder* ComponentRegistry::find(const std::string& kind,
                                                 const std::string& impl) const {
  const auto it = builders_.find({kind, impl});
  return it == builders_.end() ? nullptr : &it->second;
}
}  // namespace openhbx::config
