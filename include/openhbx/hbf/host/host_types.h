#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "openhbx/common/payload_handle.h"
#include "openhbx/common/strong_types.h"
#include "openhbx/hbf/address/hbf_address_types.h"
#include "openhbx/hbf/controller/controller_types.h"

namespace openhbx::hbf::host {

struct AxiIdTag;
using AxiId = StrongId<AxiIdTag>;

enum class PacketType : std::uint8_t {
  FlashIo = 0,
  ScratchpadIo = 1,
  CsrAdmin = 2,
  Reserved = 3,
};
enum class HostOperation { Read, Write, Admin };
enum class LinkState {
  Reset,
  InitializationPhase1,
  InitializationPhase2,
  InitializationPhase3,
  InitializationPhase4,
  Ready,
  Disabled,
  Faulted,
};
enum class HostError {
  None,
  InvalidChannel,
  InvalidAxiInterface,
  ReservedPacket,
  Misaligned,
  InvalidSize,
  CrossesDlu,
  OutOfRange,
  InvalidPayload,
  GateClosed,
  Unsupported,
  InvalidLifecycle,
};
enum class LinkTransitionResult { Accepted, InvalidChannel, StaleGeneration, InvalidTransition };

struct HostIngress {
  Generation generation;
  address::ChannelId channel;
  std::uint8_t axi_interface{0};
  AxiId axi_id;
  PacketType packet_type{PacketType::FlashIo};
  HostOperation operation{HostOperation::Read};
  std::uint64_t address_bytes{0};
  std::uint32_t size_bytes{64};
  bool batch{false};
  std::uint8_t admin_opcode{0};
  PayloadHandle payload;
};

struct HostResponse {
  Token token;
  Generation generation;
  address::ChannelId channel;
  std::uint8_t axi_interface{0};
  AxiId axi_id;
  PacketType packet_type{PacketType::FlashIo};
  HostOperation operation{HostOperation::Read};
  std::uint8_t command_status{0};
  bool data_valid{false};
  PayloadHandle payload;
  Cycle completed_at;
  controller::ControllerErrorInfo error_info{controller::ControllerErrorInfo::None};
};

using HostResponseSink = std::function<void(HostResponse)>;

}  // namespace openhbx::hbf::host
