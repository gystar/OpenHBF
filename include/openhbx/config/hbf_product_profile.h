#pragma once

#include <map>
#include <string>

#include "openhbx/config/config_types.h"

namespace openhbx::config {

struct ProfileField {
  std::string value;
  SourceMetadata source;
  bool locked = false;
};

struct ProfileDefinition {
  std::string name;
  std::map<std::string, ProfileField> fields;
};

ConfigResult<ProfileDefinition> find_hbf_product_profile(const std::string& name);

}  // namespace openhbx::config
