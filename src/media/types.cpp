#include "openhbf/media/types.h"

#include <sstream>
#include <cmath>

#include "openhbf/common/checked_math.h"

namespace openhbf::media {

bool operator==(const MediaAddress& lhs, const MediaAddress& rhs) noexcept {
  return lhs.channel == rhs.channel && lhs.core_die == rhs.core_die &&
         lhs.die == rhs.die && lhs.bank == rhs.bank &&
         lhs.block == rhs.block && lhs.page == rhs.page;
}

Result<void> validate(const Geometry& geometry) {
  if (geometry.channels == 0 || geometry.channels > kMaxHostChannels ||
      geometry.core_dies_per_channel == 0 || geometry.dies_per_core == 0 ||
      geometry.banks_per_die == 0 || geometry.blocks_per_bank == 0 ||
      geometry.pages_per_block == 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "media geometry dimensions must be non-zero and channels must be <= 16"});
  }
  const auto ncdu = geometry.core_dies_per_channel;
  if (ncdu > 16 || (ncdu & (ncdu - 1U)) != 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument,
         "media.core_dies_per_channel (NCDU) must be one of 1,2,4,8,16"});
  }
  if (geometry.banks_per_die < geometry.channels ||
      geometry.banks_per_die % geometry.channels != 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument,
         "media.banks_per_die must be divisible by channels for equal channel slices"});
  }
  std::uint64_t count = 1;
  for (const std::uint64_t dimension : {
           std::uint64_t{geometry.core_dies_per_channel},
           std::uint64_t{geometry.dies_per_core},
           std::uint64_t{geometry.banks_per_die},
           std::uint64_t{geometry.blocks_per_bank},
           std::uint64_t{geometry.pages_per_block}}) {
    auto product = checked_mul(count, dimension);
    if (!product) {
      return Result<void>::failure(product.error());
    }
    count = product.value();
  }
  return Result<void>::success();
}

ChannelId owner_channel(const Geometry& geometry,
                        const MediaAddress& address) noexcept {
  return ChannelId(static_cast<std::uint16_t>(address.bank % geometry.channels));
}

Result<void> validate(const MediaConfig& config) {
  auto geometry = validate(config.geometry);
  if (!geometry) return geometry;
  if (config.max_in_flight == 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "media.max_in_flight must be greater than zero"});
  }
  if (!std::isfinite(config.reliability.factory_bad_rate) ||
      config.reliability.factory_bad_rate < 0.0 ||
      config.reliability.factory_bad_rate > 1.0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "media.reliability.factory_bad_rate must be in [0,1]"});
  }
  if (!(config.thermal.throttle_celsius < config.thermal.cattrip_celsius) ||
      config.thermal.throttle_denominator == 0 ||
      config.thermal.throttle_numerator < config.thermal.throttle_denominator) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "invalid media thermal thresholds or multiplier"});
  }
  return Result<void>::success();
}

Result<void> validate(const MediaCommand& command, const Geometry& geometry) {
  if (command.address.channel.value() >= geometry.channels ||
      command.address.core_die >= geometry.core_dies_per_channel ||
      command.address.die >= geometry.dies_per_core ||
      command.address.bank >= geometry.banks_per_die ||
      command.address.block >= geometry.blocks_per_bank ||
      command.address.page >= geometry.pages_per_block) {
    return Result<void>::failure(
        {ErrorCode::kOutOfRange, "media command address is outside configured geometry: " + to_string(command.address)});
  }
  if (owner_channel(geometry, command.address) != command.address.channel) {
    return Result<void>::failure(
        {ErrorCode::kOutOfRange,
         "media command channel does not own the addressed NAND bank slice: " +
             to_string(command.address)});
  }
  if (command.op == MediaOp::Program && !command.payload) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "program command requires an immutable 4096-byte payload"});
  }
  if (command.op != MediaOp::Program && command.payload) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "read and erase commands must not carry a payload"});
  }
  if (command.op == MediaOp::Erase && command.address.page != 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "erase command address.page must be zero"});
  }
  if (!is_valid_token(command.origin)) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "media command requires a non-zero origin token"});
  }
  return Result<void>::success();
}

std::string to_string(const MediaAddress& address) {
  std::ostringstream out;
  out << "ch=" << address.channel.value() << "/core=" << address.core_die
      << "/die=" << address.die << "/bank=" << address.bank
      << "/block=" << address.block << "/page=" << address.page;
  return out.str();
}

const char* to_string(MediaOp op) noexcept {
  switch (op) {
    case MediaOp::Read: return "Read";
    case MediaOp::Program: return "Program";
    case MediaOp::Erase: return "Erase";
  }
  return "InvalidMediaOp";
}

const char* to_string(MediaStatus status) noexcept {
  switch (status) {
    case MediaStatus::Success: return "Success";
    case MediaStatus::InvalidAddress: return "InvalidAddress";
    case MediaStatus::InvalidCommand: return "InvalidCommand";
    case MediaStatus::Busy: return "Busy";
    case MediaStatus::ErasedPage: return "ErasedPage";
    case MediaStatus::ProgramOrderViolation: return "ProgramOrderViolation";
    case MediaStatus::ProgramFailure: return "ProgramFailure";
    case MediaStatus::EraseFailure: return "EraseFailure";
    case MediaStatus::Uncorrectable: return "Uncorrectable";
    case MediaStatus::RetryRequired: return "RetryRequired";
    case MediaStatus::BlockBad: return "BlockBad";
    case MediaStatus::CapacityRetired: return "CapacityRetired";
    case MediaStatus::DieRecovering: return "DieRecovering";
    case MediaStatus::DieFailed: return "DieFailed";
    case MediaStatus::Aborted: return "Aborted";
    case MediaStatus::InternalError: return "InternalError";
  }
  return "InvalidMediaStatus";
}

}  // namespace openhbf::media
