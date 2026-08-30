#pragma once

#include "openhbx/pal/flash_pal.h"
#include "openhbx/hbf/controller/controller_types.h"

namespace openhbx::hbf::controller {
struct EccResult {
  ControllerStatus status;
  bool data_valid;
  PayloadHandle payload;
  ControllerErrorInfo error_info{ControllerErrorInfo::None};
};
class EccPipeline {
 public:
  explicit EccPipeline(std::size_t credits) : credits_(credits) {}
  EccResult decode(const pal::PalCompletion& completion) const;
  std::size_t credits() const noexcept { return credits_; }
 private:
  std::size_t credits_;
};
}  // namespace openhbx::hbf::controller
