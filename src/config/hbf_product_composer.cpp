#include "openhbx/config/hbf_product_composer.h"

#include <array>


namespace openhbx::config {
ConfigResult<std::unique_ptr<ProductAssembly>> compose_hbf_product(
    const ResolvedHbfConfig& config, const ComponentRegistry& registry) {
  if (!registry.sealed())
    return ConfigResult<std::unique_ptr<ProductAssembly>>::failure({{
        ConfigErrorCode::BuildFailed, "components", "registry must be sealed before composition", 0}});
  const auto& selected = config.components();
  // Production dependency topology: roots first, then their consumers.
  const std::array<std::pair<const char*, const std::string*>, 6> order{{
      {"address", &selected.address}, {"media", &selected.media},
      {"interconnect", &selected.interconnect}, {"controller", &selected.controller},
      {"host", &selected.host}, {"ras", &selected.ras}}};
  std::vector<std::unique_ptr<ComponentInstance>> temporary;
  temporary.reserve(order.size());
  for (const auto& entry : order) {
    const auto* builder = registry.find(entry.first, *entry.second);
    if (builder == nullptr)
      return ConfigResult<std::unique_ptr<ProductAssembly>>::failure({{
          ConfigErrorCode::UnknownImplementation, "components." + std::string(entry.first),
          "no builder registered for " + *entry.second, 0}});
    auto built = (*builder)(config);
    if (!built) return ConfigResult<std::unique_ptr<ProductAssembly>>::failure(built.issues());
    if (!built.value())
      return ConfigResult<std::unique_ptr<ProductAssembly>>::failure({{
          ConfigErrorCode::BuildFailed, "components." + std::string(entry.first),
          "builder returned a null component", 0}});
    temporary.push_back(built.take_value());
  }
  return ConfigResult<std::unique_ptr<ProductAssembly>>::success(
      std::make_unique<ProductAssembly>(std::move(temporary)));
}

}  // namespace openhbx::config
