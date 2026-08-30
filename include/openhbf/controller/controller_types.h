#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "openhbf/common/error.h"
#include "openhbf/common/types.h"

namespace openhbf::controller {
using TxnId = StrongValue<struct TxnTag, uint64_t>;
using HostToken = StrongValue<struct HostTag, uint64_t>;
using DluIndex = uint64_t;
using SegmentIndex = uint8_t;
enum class ControllerOp { Read, WriteSegment };
enum class ControllerState { Dispatched, Accumulating, Ready, Waiting, Completed, Failed, Aborted };
enum class Status { Accepted, Busy, Completed, Aborted, Invalid, Duplicate, Mpdlu, Timeout, MissingSegment };
struct WriteSegment { ChannelId channel; DluIndex dlu; SegmentIndex segment; HostToken host; std::array<uint8_t,64> data{}; Generation generation{}; };
struct ReadyDlu { ChannelId channel; DluIndex dlu; Generation generation{}; std::array<uint8_t,4096> data{}; std::vector<HostToken> waiters; };
struct ControllerTxn { TxnId id; ChannelId channel; HostToken host; Generation generation{}; ControllerOp op{}; ControllerState state{}; };
struct ControllerEvent { TxnId txn; Generation generation{}; Status status{}; };
const char* to_string(Status) noexcept;
bool valid_transition(ControllerState from, ControllerState to) noexcept;
}
