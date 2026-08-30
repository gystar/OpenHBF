#pragma once

#include <string>

#include "openhbx/config/config_types.h"

namespace openhbx::config {

ConfigResult<RawConfigTree> parse_hbf_yaml(const std::string& text);

}  // namespace openhbx::config
