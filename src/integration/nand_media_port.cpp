#include "openhbx/integration/nand_media_port.h"

#include <utility>

#include "openhbx/media/nand/nand_flash_device.h"

namespace openhbx::integration {

AdmissionResult NandMediaPort::try_issue_media(media::FlashCommand command, Cycle now) {
  if (device_ == nullptr)
    return AdmissionResult::rejected(RejectionReason::InvalidLifecycle,
                                     "NAND media port is not connected");
  return device_->try_issue(std::move(command), now);
}

}  // namespace openhbx::integration
