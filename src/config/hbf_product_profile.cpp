#include "openhbx/config/hbf_product_profile.h"

namespace openhbx::config {
ConfigResult<ProfileDefinition> find_hbf_product_profile(const std::string& name) {
  if (name != "OCP_HBF_0_7")
    return ConfigResult<ProfileDefinition>::failure({{ConfigErrorCode::UnsupportedProfile,
        "product.profile", "TASK1 supports only OCP_HBF_0_7", 0}});
  ProfileDefinition profile;
  profile.name = name;
  const SourceMetadata ocp{ConfigOrigin::Ocp, "OCP HBF v0.7.0 profile lock"};
  profile.fields.emplace("product.family", ProfileField{"HBF", ocp, true});
  profile.fields.emplace("product.profile", ProfileField{name, ocp, true});
  profile.fields.emplace("host.transaction_bytes", ProfileField{"64 B", ocp, true});
  profile.fields.emplace("controller.dlu_bytes", ProfileField{"4096 B", ocp, true});
  return ConfigResult<ProfileDefinition>::success(std::move(profile));
}
}  // namespace openhbx::config
