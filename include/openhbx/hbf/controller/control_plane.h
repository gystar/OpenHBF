#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openhbx::hbf::controller {
enum class AdminOpcode { Reset, ZoneRemap, ReducedCapacity, SecureErase, Bist };
enum class AdminResult { Accepted, Unsupported, Invalid };
class ControlPlane {
 public:
  explicit ControlPlane(std::size_t scratchpad_bytes);
  bool write_scratchpad(std::size_t offset, const std::vector<std::uint8_t>& bytes);
  std::vector<std::uint8_t> read_scratchpad(std::size_t offset, std::size_t size) const;
  AdminResult validate(AdminOpcode opcode, bool vendor_enabled) const;
  void reset(bool clear_scratchpad);
 private:
  std::vector<std::uint8_t> scratchpad_;
};
}  // namespace openhbx::hbf::controller
