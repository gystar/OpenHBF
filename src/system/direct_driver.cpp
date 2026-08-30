#include "openhbx/system/direct_driver.h"

#include <utility>

namespace openhbx {

AdmissionResult SyntheticDriver::submit(integration::BridgeRequest request) {
  auto caller = std::move(request.completion);
  request.completion = [this, caller = std::move(caller)](
                           const SystemCompletion& completion) mutable {
    ++result_.completed;
    result_.completions.push_back(completion);
    if (caller) caller(completion);
  };
  const auto result = bridge_.try_submit(std::move(request));
  if (result.code == AdmissionCode::Accepted) ++result_.submitted;
  else if (result.code == AdmissionCode::Rejected) ++result_.rejected;
  return result;
}

DriverResult SyntheticDriver::drain(std::uint64_t cycle_budget) {
  system_.drain(cycle_budget);
  return result_;
}

}  // namespace openhbx
