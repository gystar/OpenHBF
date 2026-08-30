#pragma once

#include "openhbx/hbf/controller/base_die_flash_controller.h"

namespace openhbx::hbf::host {

class IControllerPort {
 public:
  virtual ~IControllerPort() = default;
  virtual AdmissionResult submit(controller::ControllerRequest request, Cycle now) = 0;
  virtual AdmissionResult submit_admin(Token token, Generation generation,
                                       std::uint8_t ocp_opcode, Cycle now) = 0;
  virtual void reset(Generation generation, Cycle now) = 0;
};

class BaseDieControllerPort final : public IControllerPort {
 public:
  explicit BaseDieControllerPort(controller::BaseDieFlashController& controller)
      : controller_(controller) {}
  AdmissionResult submit(controller::ControllerRequest request, Cycle now) override;
  AdmissionResult submit_admin(Token token, Generation generation,
                               std::uint8_t ocp_opcode, Cycle now) override;
  void reset(Generation generation, Cycle now) override;
 private:
  controller::BaseDieFlashController& controller_;
};

}  // namespace openhbx::hbf::host
