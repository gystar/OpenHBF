#pragma once

#include "openhbx/pal/media_port.h"

namespace openhbx::media::nand { class NandFlashDevice; }

namespace openhbx::integration {

class NandMediaPort final : public pal::IFlashMediaPort {
 public:
  NandMediaPort() = default;
  explicit NandMediaPort(media::nand::NandFlashDevice& device) : device_(&device) {}
  void connect(media::nand::NandFlashDevice& device) noexcept { device_ = &device; }
  AdmissionResult try_issue_media(media::FlashCommand command, Cycle now) override;

 private:
  media::nand::NandFlashDevice* device_{nullptr};
};

}  // namespace openhbx::integration
