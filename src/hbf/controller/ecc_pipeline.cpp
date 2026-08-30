#include "openhbx/hbf/controller/ecc_pipeline.h"

namespace openhbx::hbf::controller {
EccResult EccPipeline::decode(const pal::PalCompletion& c) const {
  switch (c.status) {
    case pal::PalStatus::Success:
      if (!c.data_valid || c.payload.size() != 4096)
        return {ControllerStatus::Invalid, false, {}};
      return {ControllerStatus::Success, true, c.payload};
    case pal::PalStatus::ReadErasedPage:
      return {ControllerStatus::ErasedPage, false, {}};
    case pal::PalStatus::RawCorrectable:
    case pal::PalStatus::RefreshNotice:
    case pal::PalStatus::Corrected:
      if (!c.data_valid || c.payload.size() != 4096)
        return {ControllerStatus::Invalid, false, {}};
      return {ControllerStatus::Corrected, true, c.payload};
    case pal::PalStatus::RawUncorrectable:
    case pal::PalStatus::Uncorrectable:
      return {ControllerStatus::Uncorrectable, false, {}};
    case pal::PalStatus::RetrySuggested:
      // OCP status is known, but retry-stage ErrorInfo remains unavailable
      // until the Media/PAL completion contract carries that typed field.
      return {ControllerStatus::Retry, false, {},
              ControllerErrorInfo::RetryStageUnavailable};
    case pal::PalStatus::CapacityUnusable:
      return {ControllerStatus::CapacityUnusable, false, {}};
    case pal::PalStatus::DieTemporarilyBlocked:
      return {ControllerStatus::DieTemporarilyBlocked, false, {}};
    case pal::PalStatus::ProgramFail:
    case pal::PalStatus::EraseFail: return {ControllerStatus::ProgramFail, false, {}};
    case pal::PalStatus::Aborted: return {ControllerStatus::Aborted, false, {}};
    case pal::PalStatus::InvalidAddress:
    case pal::PalStatus::InvalidState:
    case pal::PalStatus::InvalidPayload:
    case pal::PalStatus::MediaIntegrityError:
      return {ControllerStatus::Invalid, false, {}};
    case pal::PalStatus::BadBlock:
    case pal::PalStatus::MediaRejected:
    case pal::PalStatus::PathUnavailable:
    case pal::PalStatus::InternalError:
      return {ControllerStatus::UnsupportedSpecGap, false, {}};
  }
  return {ControllerStatus::UnsupportedSpecGap, false, {}};
}
}  // namespace openhbx::hbf::controller
