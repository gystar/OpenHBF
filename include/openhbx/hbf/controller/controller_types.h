#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "openhbx/common/payload_handle.h"
#include "openhbx/common/strong_types.h"
#include "openhbx/hbf/address/hbf_address_types.h"
#include "openhbx/media/types.h"

namespace openhbx::hbf::controller {

enum class ControllerStatus : std::uint8_t {
  Success = 0x0, Overlap = 0x2, PendingLimit = 0x4, AccumulationTimeout = 0x5,
  OrderViolation = 0x6, ProgramFail = 0x7, CapacityUnusable = 0x8,
  DieTemporarilyBlocked = 0x9, PendingDataMissing = 0xA,
  // Read and Write status share the OCP four-bit wire field; meaning is
  // selected by command context (Tables 12 and 13).
  Uncorrectable = 0x4, Corrected = 0x5, Retry = 0x6, ErasedPage = 0x7,
  Unsupported = 0xD, Aborted = 0xE, Invalid = 0xF, UnsupportedSpecGap = 0xFF
};
enum class HostOperation { ReadSector, WriteSector, ReadDlu };
enum class ReadMode { Regular, Batch };
enum class ControllerErrorInfo { None, RetryStageUnavailable };

struct DluKey {
  address::BlockKey block;
  address::PageIndex page;
  friend bool operator<(const DluKey& a, const DluKey& b) noexcept {
    if (a.block < b.block) return true;
    if (b.block < a.block) return false;
    return a.page < b.page;
  }
  friend bool operator==(const DluKey& a, const DluKey& b) noexcept {
    return !(a < b) && !(b < a);
  }
};

struct ControllerRequest {
  Token token;
  Generation generation;
  HostOperation operation{HostOperation::ReadDlu};
  DluKey dlu;
  address::PhysicalBank bank;
  address::SectorIndex sector;
  std::uint64_t endpoint{0};
  ReadMode read_mode{ReadMode::Regular};
  PayloadHandle payload;
};

struct ControllerCompletion {
  Token token;
  Generation generation;
  ControllerStatus status{ControllerStatus::Invalid};
  bool data_valid{false};
  PayloadHandle payload;
  Cycle completed_at;
  ControllerErrorInfo error_info{ControllerErrorInfo::None};
};

using ControllerCompletionSink = std::function<void(ControllerCompletion)>;

inline media::PhysicalAddress physical_address(const ControllerRequest& request) {
  return {request.bank.core_die.value(), request.bank.die.value(),
          request.bank.bank.value(), request.dlu.block.block.value(),
          request.dlu.page.value()};
}

}  // namespace openhbx::hbf::controller
