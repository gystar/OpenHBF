#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "openhbx/config/config_types.h"
#include "openhbx/config/resolved_hbf_config.h"

namespace openhbx::config {

class ComponentInstance {
 public:
  virtual ~ComponentInstance() = default;
};
using ComponentBuilder = std::function<ConfigResult<std::unique_ptr<ComponentInstance>>(
    const ResolvedHbfConfig&)>;

class ComponentRegistry {
 public:
  std::vector<ConfigIssue> add(std::string kind, std::string impl,
                               ComponentBuilder builder);
  const ComponentBuilder* find(const std::string& kind,
                               const std::string& impl) const;
  void seal() noexcept { sealed_ = true; }
  bool sealed() const noexcept { return sealed_; }

 private:
  std::map<std::pair<std::string, std::string>, ComponentBuilder> builders_;
  bool sealed_ = false;
};

}  // namespace openhbx::config
