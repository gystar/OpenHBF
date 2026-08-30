#include "openhbx/integration/request_bridge.h"

#include <limits>
#include <utility>
#include <vector>

namespace openhbx::integration {

RequestBridge::RequestBridge(OpenHbxSystem& system) : system_(system) {
  next_token_ = system_.allocate_request_token().value();
}

AdmissionResult RequestBridge::try_submit(BridgeRequest request) {
  if (request.address < 0 || request.size_bytes <= 0 ||
      request.size_bytes > 4096 || !request.completion || next_token_ == 0)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  if (request.ingress_id < 0 || request.ingress_id > 3 || request.source_id < 0)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);

  hbf::host::HostIngress ingress;
  ingress.generation = system_.generation();
  const auto channel_capacity =
      system_.resolved_config().geometry().capacity_bytes /
      system_.resolved_config().geometry().host_channels;
  const auto axi_count = system_.axi_interfaces();
  const auto axi_capacity = channel_capacity / axi_count;
  const auto address = static_cast<std::uint64_t>(request.address);
  if (channel_capacity == 0 || axi_capacity == 0 ||
      address >= system_.resolved_config().geometry().capacity_bytes)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  ingress.channel = hbf::address::ChannelId(address / channel_capacity);
  const auto channel_local = address % channel_capacity;
  ingress.axi_interface = static_cast<std::uint8_t>(channel_local / axi_capacity);
  ingress.address_bytes = channel_local % axi_capacity;
  ingress.axi_id = hbf::host::AxiId(static_cast<std::uint64_t>(request.source_id) + 1);
  ingress.packet_type = hbf::host::PacketType::FlashIo;
  ingress.operation = request.type == BridgeRequestType::Read
      ? hbf::host::HostOperation::Read : hbf::host::HostOperation::Write;
  ingress.size_bytes = static_cast<std::uint32_t>(request.size_bytes);
  ingress.payload = request.payload;

  const Token token(next_token_);
  auto callback = std::move(request.completion);
  SystemRequest system_request{token, std::move(ingress),
      [callback = std::move(callback)](SystemCompletion completion) mutable {
        callback(completion);
      }};
  const auto result = system_.try_submit(std::move(system_request));
  if (result.code == AdmissionCode::Accepted) {
    next_token_ = system_.allocate_request_token().value();
  }
  return result;
}

}  // namespace openhbx::integration
