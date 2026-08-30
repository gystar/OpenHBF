#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/common/event.h"
#include "openhbf/common/types.h"

namespace openhbf::media {

constexpr std::size_t kPageBytes = 4096;
inline constexpr std::uint16_t kMaxHostChannels = 16;
inline constexpr std::uint16_t kMaxChannelsPerCoreDie = 16;
inline constexpr std::uint16_t kDefaultNcdu = 1;

struct MediaTokenTag;
using MediaToken = StrongValue<MediaTokenTag, std::uint64_t>;

enum class MediaOp : std::uint8_t { Read, Program, Erase };
enum class MediaStatus : std::uint8_t {
  Success,
  InvalidAddress,
  InvalidCommand,
  Busy,
  ErasedPage,
  ProgramOrderViolation,
  ProgramFailure,
  EraseFailure,
  Uncorrectable,
  RetryRequired,
  BlockBad,
  CapacityRetired,
  DieRecovering,
  DieFailed,
  Aborted,
  InternalError,
};
enum class SubmitState : std::uint8_t { Accepted, Busy, Rejected };
enum class PayloadMode : std::uint8_t { FunctionalSparse, TimingOnly };
enum class CellMode : std::uint8_t { Slc, Mlc, Tlc, Qlc };
enum class PageClass : std::uint8_t { Default, Lsb, Csb, Msb, Vendor };
enum class StageKind : std::uint8_t {
  ReadSense,
  ReadDataOut,
  ProgramDataIn,
  ProgramArray,
  ProgramVerify,
  EraseArray,
  EraseVerify,
};

// Channel is an ownership label for a slice of the physical
// CoreDie/Die/Bank/Block/Page address, not a physical parent dimension.
// Vendor-only Chip, Plane and cell-page details are intentionally absent.
struct MediaAddress {
  ChannelId channel{};
  std::uint16_t core_die = 0;
  std::uint16_t die = 0;
  std::uint16_t bank = 0;
  std::uint32_t block = 0;
  std::uint32_t page = 0;

  friend bool operator==(const MediaAddress& lhs,
                         const MediaAddress& rhs) noexcept;
  friend bool operator!=(const MediaAddress& lhs,
                         const MediaAddress& rhs) noexcept {
    return !(lhs == rhs);
  }
};

using PageData = std::array<std::uint8_t, kPageBytes>;
using SharedPage = std::shared_ptr<const PageData>;

struct PayloadSnapshot {
  MediaAddress address;
  std::uint64_t block_epoch = 0;
  bool materialized = false;
  std::uint64_t signature = 0;
  SharedPage bytes;
};

struct MediaCommand {
  MediaOp op = MediaOp::Read;
  MediaAddress address;
  Generation generation{};
  RequestToken origin{};
  SharedPage payload;
  PageClass page_class = PageClass::Default;
};

struct MediaCompletion {
  MediaToken token{};
  RequestToken origin{};
  Generation generation{};
  MediaOp op = MediaOp::Read;
  MediaAddress address;
  MediaStatus status = MediaStatus::InternalError;
  Cycle accepted_cycle{};
  Cycle completed_cycle{};
  std::optional<PayloadSnapshot> payload;
  std::uint32_t raw_errors = 0;
  bool refresh_recommended = false;
};

using CompletionSink = std::function<void(MediaCompletion)>;

struct IssueResult {
  SubmitState state = SubmitState::Rejected;
  MediaToken token{};
  MediaStatus status = MediaStatus::InvalidCommand;
  std::string detail;
};

struct StageEvent {
  MediaToken token{};
  Generation generation{};
  std::uint32_t stage_index = 0;
};

struct Geometry {
  std::uint16_t channels = 1;
  // OCP BUCCAP.NCDU compatibility field. These are physical Core Die domains,
  // not per-channel copies. The name remains temporarily for source stability.
  std::uint16_t core_dies_per_channel = kDefaultNcdu;
  std::uint16_t dies_per_core = 1;
  std::uint16_t banks_per_die = 1;
  std::uint32_t blocks_per_bank = 1;
  std::uint32_t pages_per_block = 1;
};

struct ReliabilityConfig {
  std::uint64_t seed = 1;
  // Factory bad generation is an explicit synthetic profile choice. Zero
  // disables generation; OCP does not prescribe a default percentage.
  double factory_bad_rate = 0.0;
  std::vector<MediaAddress> factory_bad_blocks;
  std::uint64_t refresh_read_threshold = 0;
  std::uint32_t corrected_error_threshold = 0;
  std::uint32_t retry_error_threshold = 0;
};

struct ThermalConfig {
  double throttle_celsius = 95.0;
  double cattrip_celsius = 105.0;
  std::uint32_t throttle_numerator = 2;
  std::uint32_t throttle_denominator = 1;
};

struct MediaConfig {
  Geometry geometry;
  PayloadMode payload_mode = PayloadMode::FunctionalSparse;
  CellMode cell_mode = CellMode::Slc;
  std::size_t max_in_flight = 256;
  ReliabilityConfig reliability;
  ThermalConfig thermal;
  std::size_t log_capacity = 1024;
};

Result<void> validate(const Geometry& geometry);
ChannelId owner_channel(const Geometry& geometry,
                        const MediaAddress& address) noexcept;
Result<void> validate(const MediaConfig& config);
Result<void> validate(const MediaCommand& command, const Geometry& geometry);
std::string to_string(const MediaAddress& address);
const char* to_string(MediaOp op) noexcept;
const char* to_string(MediaStatus status) noexcept;

}  // namespace openhbf::media
