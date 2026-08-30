#pragma once

#include <cstdint>
#include <vector>

#include "openhbx/integration/request_bridge.h"

namespace openhbx {

struct DriverResult {
  std::uint64_t submitted{0};
  std::uint64_t completed{0};
  std::uint64_t rejected{0};
  std::vector<SystemCompletion> completions;
};

class SyntheticDriver {
 public:
  explicit SyntheticDriver(OpenHbxSystem& system) : system_(system), bridge_(system) {}
  AdmissionResult submit(integration::BridgeRequest request);
  DriverResult drain(std::uint64_t cycle_budget);

 private:
  OpenHbxSystem& system_;
  integration::RequestBridge bridge_;
  DriverResult result_;
};

}  // namespace openhbx
