#include "openhbx/config/config_types.h"

namespace openhbx::config {
const char* to_string(ConfigOrigin origin) noexcept {
  switch (origin) {
    case ConfigOrigin::Ocp: return "ocp";
    case ConfigOrigin::Vendor: return "vendor";
    case ConfigOrigin::Synthetic: return "synthetic";
    case ConfigOrigin::User: return "user";
    case ConfigOrigin::Derived: return "derived";
  }
  return "unknown";
}
const char* to_string(ConfigErrorCode code) noexcept {
  switch (code) {
    case ConfigErrorCode::ParseError: return "ParseError";
    case ConfigErrorCode::DuplicateKey: return "DuplicateKey";
    case ConfigErrorCode::UnknownKey: return "UnknownKey";
    case ConfigErrorCode::MissingField: return "MissingField";
    case ConfigErrorCode::TypeMismatch: return "TypeMismatch";
    case ConfigErrorCode::UnitRequired: return "UnitRequired";
    case ConfigErrorCode::OutOfRange: return "OutOfRange";
    case ConfigErrorCode::LockedField: return "LockedField";
    case ConfigErrorCode::UnsupportedProfile: return "UnsupportedProfile";
    case ConfigErrorCode::UnknownImplementation: return "UnknownImplementation";
    case ConfigErrorCode::CapabilityMismatch: return "CapabilityMismatch";
    case ConfigErrorCode::ArithmeticOverflow: return "ArithmeticOverflow";
    case ConfigErrorCode::DuplicateRegistration: return "DuplicateRegistration";
    case ConfigErrorCode::RegistrySealed: return "RegistrySealed";
    case ConfigErrorCode::BuildFailed: return "BuildFailed";
  }
  return "Unknown";
}
}  // namespace openhbx::config
