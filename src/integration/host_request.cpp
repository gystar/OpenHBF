#include "openhbf/integration/host_request.h"

#include <sstream>

namespace openhbf::integration {
namespace {
Result<void> invalid(std::string message) {
  return Result<void>::failure({ErrorCode::kInvalidArgument, std::move(message)});
}
}  // namespace

Result<void> validate(const HostRequest& request, std::uint16_t channels,
                      std::uint16_t axi_interfaces_per_channel,
                      PayloadMode payload_mode) {
  if (channels == 0 || channels > 16) {
    return invalid("host.channels=" + std::to_string(channels) +
                   " must be in [1,16]");
  }
  if (is_valid_token(request.token))
    return invalid("request.token must be zero before system admission");
  if (axi_interfaces_per_channel != 1 && axi_interfaces_per_channel != 2 &&
      axi_interfaces_per_channel != 4) {
    return invalid("host.axi_interfaces_per_channel=" +
                   std::to_string(axi_interfaces_per_channel) +
                   " must be 1, 2, or 4");
  }
  if (request.channel.value() >= channels) {
    return invalid("request.channel=" + std::to_string(request.channel.value()) +
                   " exceeds configured channels=" + std::to_string(channels));
  }
  if (request.size_bytes == 0 || request.size_bytes > 4096 ||
      request.size_bytes % 64 != 0) {
    return invalid("request.size_bytes=" + std::to_string(request.size_bytes) +
                   " must be a 64-byte multiple in [64,4096]");
  }
  if (request.address % 64 != 0) {
    return invalid("request.address=" + std::to_string(request.address) +
                   " must be 64-byte aligned");
  }
  const std::uint64_t offset = request.address % 4096;
  if (offset + request.size_bytes > 4096) {
    return invalid("request address/size crosses a 4096-byte DLU boundary");
  }
  if (request.opcode == HostOpcode::Write) {
    if (!request.payload && payload_mode == PayloadMode::Functional)
      return invalid("write request payload is null in functional payload mode");
    if (request.payload && request.payload->size() != request.size_bytes) {
      return invalid("write payload bytes=" +
                     std::to_string(request.payload->size()) +
                     " does not equal request.size_bytes=" +
                     std::to_string(request.size_bytes));
    }
  } else if (request.payload) {
    return invalid("read request payload must be null");
  }
  return Result<void>::success();
}

const char* to_string(HostOpcode opcode) noexcept {
  return opcode == HostOpcode::Read ? "read" : "write";
}
const char* to_string(HostStatus status) noexcept {
  switch (status) {
    case HostStatus::Success: return "success";
    case HostStatus::Aborted: return "aborted";
    case HostStatus::InvalidRequest: return "invalid_request";
    case HostStatus::ReadCecc: return "read_cecc";
    case HostStatus::ReadUecc: return "read_uecc";
    case HostStatus::ProgramFailed: return "program_failed";
    case HostStatus::InternalError: return "internal_error";
  }
  return "unknown";
}
std::string describe(const HostRequest& request) {
  std::ostringstream out;
  out << "opcode=" << to_string(request.opcode) << ",address=" << request.address
      << ",size_bytes=" << request.size_bytes
      << ",channel=" << request.channel.value() << ",axi_id=" << request.axi_id
      << ",source_id=" << request.source_id
      << ",ingress_id=" << request.ingress_id;
  return out.str();
}
}  // namespace openhbf::integration
