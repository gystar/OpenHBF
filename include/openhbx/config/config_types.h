#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openhbx::config {

enum class ConfigOrigin { Ocp, Vendor, Synthetic, User, Derived };
enum class ConfigErrorCode {
  ParseError,
  DuplicateKey,
  UnknownKey,
  MissingField,
  TypeMismatch,
  UnitRequired,
  OutOfRange,
  LockedField,
  UnsupportedProfile,
  UnknownImplementation,
  CapabilityMismatch,
  ArithmeticOverflow,
  DuplicateRegistration,
  RegistrySealed,
  BuildFailed,
};

struct ConfigIssue {
  ConfigErrorCode code;
  std::string path;
  std::string message;
  std::size_t line = 0;
};

struct RawConfigTree {
  std::map<std::string, std::string> scalars;
};

struct SourceMetadata {
  ConfigOrigin origin;
  std::string detail;
};

template <typename T>
class ConfigResult {
 public:
  static ConfigResult success(T value) {
    ConfigResult result;
    result.value_ = std::move(value);
    return result;
  }
  static ConfigResult failure(std::vector<ConfigIssue> issues) {
    ConfigResult result;
    result.issues_ = std::move(issues);
    return result;
  }
  explicit operator bool() const noexcept { return value_.has_value(); }
  const T& value() const { return value_.value(); }
  T&& take_value() { return std::move(value_.value()); }
  const std::vector<ConfigIssue>& issues() const noexcept { return issues_; }

 private:
  std::optional<T> value_;
  std::vector<ConfigIssue> issues_;
};

const char* to_string(ConfigOrigin origin) noexcept;
const char* to_string(ConfigErrorCode code) noexcept;

}  // namespace openhbx::config
