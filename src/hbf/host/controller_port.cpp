#include "openhbx/hbf/host/controller_port.h"

#include <utility>

namespace openhbx::hbf::host {
AdmissionResult BaseDieControllerPort::submit(controller::ControllerRequest request, Cycle now) {
  return controller_.submit(std::move(request), now);
}
AdmissionResult BaseDieControllerPort::submit_admin(Token token, Generation generation,
    std::uint8_t opcode, Cycle now) {
  controller::AdminOpcode command;
  switch (opcode) {
    case 0x03: command = controller::AdminOpcode::SecureErase; break;
    case 0x07: command = controller::AdminOpcode::Bist; break;
    case 0x08: command = controller::AdminOpcode::ZoneRemap; break;
    case 0x0A: command = controller::AdminOpcode::ReducedCapacity; break;
    // S6 has no implementation port for these confirmed OCP commands. Its
    // non-vendor BIST path is used only as a typed Unsupported terminal sink;
    // no BIST or alternate command is executed by ControlPlane.
    case 0x01: case 0x02: case 0x04: case 0x09: case 0x20:
      command = controller::AdminOpcode::Bist; break;
    default: return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  }
  return controller_.submit_admin(token, generation, command, false, now);
}
void BaseDieControllerPort::reset(Generation generation, Cycle now) {
  controller_.reset(generation, now);
}
}  // namespace openhbx::hbf::host
