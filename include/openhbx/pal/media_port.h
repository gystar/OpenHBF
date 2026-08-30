#pragma once

#include "openhbx/common/admission.h"
#include "openhbx/media/types.h"

namespace openhbx::pal {

class IFlashMediaPort {
 public:
  virtual ~IFlashMediaPort() = default;
  virtual AdmissionResult try_issue_media(media::FlashCommand command, Cycle now) = 0;
};

}  // namespace openhbx::pal
