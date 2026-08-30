#pragma once

#include "openhbx/hbf/host/host_types.h"

namespace openhbx::hbf::host {

struct HostProfile {
  std::uint8_t channels{1};
  std::uint8_t axi_interfaces{1};
  std::size_t queue_depth_per_interface{1};
  std::uint64_t channel_capacity_bytes{0};
};

struct ValidationResult {
  HostError error{HostError::None};
  explicit operator bool() const noexcept { return error == HostError::None; }
};

class HostValidator {
 public:
  explicit HostValidator(HostProfile profile) : profile_(profile) {}
  ValidationResult validate(const HostIngress& request) const noexcept;
  const HostProfile& profile() const noexcept { return profile_; }
  std::uint64_t axi_capacity_bytes() const noexcept {
    return profile_.axi_interfaces == 0 ? 0
        : profile_.channel_capacity_bytes / profile_.axi_interfaces;
  }

 private:
  HostProfile profile_;
};

}  // namespace openhbx::hbf::host
