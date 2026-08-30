#include "openhbx/hbf/host/packet_codec.h"

namespace openhbx::hbf::host {

std::optional<PacketType> decode_packet_type(std::uint8_t bits) noexcept {
  if (bits > 3 || bits == 3) return std::nullopt;
  return static_cast<PacketType>(bits);
}

std::optional<AdminOpcode> decode_admin_opcode(std::uint8_t opcode) noexcept {
  switch (opcode) {
    case 0x01: return AdminOpcode::SetFeature;
    case 0x02: return AdminOpcode::GetFeature;
    case 0x03: return AdminOpcode::SecureErase;
    case 0x04: return AdminOpcode::GetLogPage;
    case 0x07: return AdminOpcode::Bist;
    case 0x08: return AdminOpcode::ZoneRemapping;
    case 0x09: return AdminOpcode::ReadUcieErrors;
    case 0x0A: return AdminOpcode::ReducedCapacity;
    case 0x20: return AdminOpcode::RegisterAccess;
    default: return std::nullopt;
  }
}

bool valid_sideband_command(SidebandCommand command) noexcept {
  switch (command) {
    case SidebandCommand::MemoryRead32:
    case SidebandCommand::MemoryWrite32:
    case SidebandCommand::MemoryRead64:
    case SidebandCommand::MemoryWrite64:
    case SidebandCommand::Dms:
    case SidebandCommand::ConfigurationRead:
    case SidebandCommand::ConfigurationWrite:
    case SidebandCommand::Completion:
    case SidebandCommand::Message:
    case SidebandCommand::VendorSpecific:
    case SidebandCommand::ManagementPort: return true;
  }
  return false;
}

std::optional<HostResponse> encode_response(
    const HostIngress& request, Token token,
    const controller::ControllerCompletion& completion) noexcept {
  const auto status = static_cast<std::uint8_t>(completion.status);
  if (status > 0x0F) return std::nullopt;
  if (completion.data_valid && completion.payload.empty()) return std::nullopt;
  if (!completion.data_valid && !completion.payload.empty()) return std::nullopt;
  const bool read = request.operation == HostOperation::Read;
  if (!read && completion.data_valid) return std::nullopt;
  if (read && completion.status == controller::ControllerStatus::Success &&
      !completion.data_valid) return std::nullopt;
  HostResponse response;
  response.token = token; response.generation = request.generation;
  response.channel = request.channel; response.axi_interface = request.axi_interface;
  response.axi_id = request.axi_id; response.packet_type = request.packet_type;
  response.operation = request.operation; response.command_status = status;
  response.data_valid = completion.data_valid;
  response.payload = completion.data_valid ? completion.payload : PayloadHandle{};
  response.completed_at = completion.completed_at; response.error_info = completion.error_info;
  return response;
}

}  // namespace openhbx::hbf::host
