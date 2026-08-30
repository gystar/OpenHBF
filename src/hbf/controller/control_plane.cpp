#include "openhbx/hbf/controller/control_plane.h"

#include <algorithm>

namespace openhbx::hbf::controller {
ControlPlane::ControlPlane(std::size_t bytes) : scratchpad_(bytes) {}
bool ControlPlane::write_scratchpad(std::size_t offset, const std::vector<std::uint8_t>& bytes) {
  if (offset > scratchpad_.size() || bytes.size() > scratchpad_.size() - offset) return false;
  std::copy(bytes.begin(), bytes.end(), scratchpad_.begin() + static_cast<std::ptrdiff_t>(offset)); return true;
}
std::vector<std::uint8_t> ControlPlane::read_scratchpad(std::size_t offset, std::size_t size) const {
  if (offset > scratchpad_.size() || size > scratchpad_.size() - offset) return {};
  return {scratchpad_.begin() + static_cast<std::ptrdiff_t>(offset),
          scratchpad_.begin() + static_cast<std::ptrdiff_t>(offset + size)};
}
AdminResult ControlPlane::validate(AdminOpcode opcode, bool vendor_enabled) const {
  if (opcode == AdminOpcode::Reset || opcode == AdminOpcode::ZoneRemap ||
      opcode == AdminOpcode::ReducedCapacity) return AdminResult::Accepted;
  return vendor_enabled ? AdminResult::Accepted : AdminResult::Unsupported;
}
void ControlPlane::reset(bool clear) { if (clear) std::fill(scratchpad_.begin(), scratchpad_.end(), 0); }
}  // namespace openhbx::hbf::controller
