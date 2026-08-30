#pragma once

#include <string>
#include <utility>

namespace openhbx {

enum class AdmissionCode { Accepted, Busy, Rejected };
enum class RejectionReason { None, InvalidArgument, InvalidLifecycle, CapacityImpossible };

struct AdmissionResult {
  AdmissionCode code{AdmissionCode::Rejected};
  RejectionReason reason{RejectionReason::None};
  std::string detail;
  static AdmissionResult accepted() { return {AdmissionCode::Accepted, RejectionReason::None, {}}; }
  static AdmissionResult busy() { return {AdmissionCode::Busy, RejectionReason::None, {}}; }
  static AdmissionResult rejected(RejectionReason reason, std::string detail = {}) {
    return {AdmissionCode::Rejected, reason, std::move(detail)};
  }
};

}  // namespace openhbx
