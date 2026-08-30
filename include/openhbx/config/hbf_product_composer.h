#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>

#include "openhbx/config/component_registry.h"

namespace openhbx { class OpenHbxSystem; }

namespace openhbx::config {

class SystemComponentFactory final : public ComponentInstance {
 public:
  using Build = std::function<bool(OpenHbxSystem&, std::string&)>;
  SystemComponentFactory(std::string kind, std::string impl,
                         std::vector<std::string> dependencies, Build build)
      : kind_(std::move(kind)), impl_(std::move(impl)),
        dependencies_(std::move(dependencies)), build_(std::move(build)) {}
  const std::string& kind() const noexcept { return kind_; }
  const std::string& impl() const noexcept { return impl_; }
  const std::vector<std::string>& dependencies() const noexcept {
    return dependencies_;
  }
  bool build(OpenHbxSystem& system, std::string& error) const {
    return build_ && build_(system, error);
  }

 private:
  std::string kind_;
  std::string impl_;
  std::vector<std::string> dependencies_;
  Build build_;
};

}  // namespace openhbx::config

namespace openhbx::config {

class ProductAssembly {
 public:
  explicit ProductAssembly(std::vector<std::unique_ptr<ComponentInstance>> components)
      : components_(std::move(components)) {}
  std::size_t size() const noexcept { return components_.size(); }
  std::vector<std::unique_ptr<ComponentInstance>> take_components() {
    return std::move(components_);
  }

 private:
  std::vector<std::unique_ptr<ComponentInstance>> components_;
};

ConfigResult<std::unique_ptr<ProductAssembly>> compose_hbf_product(
    const ResolvedHbfConfig& config, const ComponentRegistry& registry);

using SystemAssemblyBuilder = std::function<ConfigResult<std::unique_ptr<OpenHbxSystem>>(
    const ResolvedHbfConfig&, std::unique_ptr<ProductAssembly>)>;
ConfigResult<std::unique_ptr<OpenHbxSystem>> compose_open_hbx_system(
    const ResolvedHbfConfig& config, const ComponentRegistry& registry,
    SystemAssemblyBuilder builder);

}  // namespace openhbx::config
