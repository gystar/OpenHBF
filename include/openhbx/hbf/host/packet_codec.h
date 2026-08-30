#pragma once

#include <cstdint>
#include <optional>

#include "openhbx/hbf/host/host_types.h"

namespace openhbx::hbf::host {

enum class AdminOpcode : std::uint8_t {
  SetFeature = 0x01, GetFeature = 0x02, SecureErase = 0x03,
  GetLogPage = 0x04, Bist = 0x07, ZoneRemapping = 0x08,
  ReadUcieErrors = 0x09, ReducedCapacity = 0x0A, RegisterAccess = 0x20,
};

// Symbolic transaction-level categories. Numeric framing is deliberately not
// exposed until the external UCIe v3.0 Table 14 values are extractable.
enum class SidebandCommand {
  MemoryRead32, MemoryWrite32, MemoryRead64, MemoryWrite64, Dms,
  ConfigurationRead, ConfigurationWrite, Completion, Message,
  VendorSpecific, ManagementPort,
};

std::optional<PacketType> decode_packet_type(std::uint8_t user_bits) noexcept;
std::optional<AdminOpcode> decode_admin_opcode(std::uint8_t opcode) noexcept;
bool valid_sideband_command(SidebandCommand command) noexcept;
std::optional<HostResponse> encode_response(
    const HostIngress& request, Token token,
    const controller::ControllerCompletion& completion) noexcept;
constexpr bool wire_codec_available() noexcept { return false; }

}  // namespace openhbx::hbf::host
