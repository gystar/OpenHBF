#include "openhbx/hbf/host/register_port.h"

#include <array>
#include <utility>

namespace openhbx::hbf::host {
namespace {
constexpr std::uint16_t kBucc = 0x000C;
constexpr std::uint16_t kBucsts = 0x0010;
constexpr std::uint16_t kBucr = 0x0014;
constexpr std::uint16_t kTmon = 0x0018;
constexpr std::uint64_t kResetMagic = 0x484246;
constexpr std::array<RegisterDescriptor, 19> kRegisters{{
  {0x0000, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x0008, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x000C, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0010, RegisterAccess::ReadOnly, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0014, RegisterAccess::WriteTrigger, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0018, RegisterAccess::ReadOnly, ResetDomain::Sticky, RegisterOwner::PerChannel, 1},
  {0x001C, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerAxi, 64},
  {0x0024, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerAxi, 1},
  {0x0034, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerAxi, 4096},
  {0x003C, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerAxi, 1},
  {0x004C, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x0054, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x005C, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x0064, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0100, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0140, RegisterAccess::ReadWrite, ResetDomain::HbfReset, RegisterOwner::PerChannel, 1},
  {0x0144, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x0148, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
  {0x014C, RegisterAccess::ReadOnly, ResetDomain::PowerOn, RegisterOwner::PerChannel, 1},
}};
}

std::optional<RegisterDescriptor> register_descriptor(std::uint16_t offset) noexcept {
  for (const auto& descriptor : kRegisters)
    if (descriptor.offset == offset) return descriptor;
  // TTTEMP is included separately to keep the fixed array audit obvious.
  if (offset == 0x0150)
    return RegisterDescriptor{0x0150, RegisterAccess::ReadOnly, ResetDomain::PowerOn,
                              RegisterOwner::PerChannel, 1};
  return std::nullopt;
}

RegisterPort::RegisterPort(std::uint8_t channels, std::uint8_t axi_interfaces,
                           std::uint64_t axi_capacity_bytes,
                           ResetIntentSink reset_intent)
    : axi_interfaces_(axi_interfaces), axi_capacity_bytes_(axi_capacity_bytes),
      reset_intent_(std::move(reset_intent)),
      channels_(channels) {
  for (auto& channel : channels_) {
    channel.per_axi.resize(axi_interfaces_); reset_image(channel);
  }
}

void RegisterPort::reset_image(ChannelImage& image) {
  image.enabled = false; image.ready = false;
  for (auto& values : image.per_axi) {
    values.clear();
    values[0x001C] = 0;  // SMEMSA, byte address
    values[0x0024] = 0;  // SMEMSZ, 64 B units
    values[0x0034] = 0;  // FIOSA, byte address
    values[0x003C] = axi_capacity_bytes_ / 4096;  // FIOSZ, 4 KiB units
  }
}

RegisterResult RegisterPort::access(const RegisterTxn& txn) {
  if (txn.channel.value() >= channels_.size() || txn.axi_interface >= axi_interfaces_)
    return {RegisterStatus::WrongOwner, 0};
  auto& image = channels_[txn.channel.value()];
  const auto descriptor = register_descriptor(txn.offset);
  if (!descriptor) return {RegisterStatus::UnknownOffset, 0};
  switch (txn.offset) {
    case kBucc:
      if (!txn.write) return {RegisterStatus::Success, image.enabled ? 1U : 0U};
      if (txn.value > 1) return {RegisterStatus::ReservedBits, image.enabled ? 1U : 0U};
      image.enabled = txn.value != 0; return {RegisterStatus::Success, txn.value};
    case kBucsts:
      if (txn.write) return {RegisterStatus::ReadOnly, image.ready ? 1U : 0U};
      return {RegisterStatus::Success, image.ready ? 1U : 0U};
    case kBucr:
      if (!txn.write) return {RegisterStatus::Success, 0};
      if (txn.value != kResetMagic) return {RegisterStatus::ReservedBits, 0};
      if (reset_intent_) reset_intent_(txn.channel);
      return {RegisterStatus::Success, 0};
    case kTmon:
      if (txn.write) return {RegisterStatus::ReadOnly, image.critical_error ? 1U : 0U};
      return {RegisterStatus::Success, image.critical_error ? 1U : 0U};
    default:
      if (descriptor->access == RegisterAccess::ReadOnly) {
        if (txn.write) return {RegisterStatus::ReadOnly, 0};
        return {RegisterStatus::Success, 0};
      }
      if (descriptor->owner == RegisterOwner::PerAxi) {
        auto& value = image.per_axi[txn.axi_interface][txn.offset];
        if (!txn.write) return {RegisterStatus::Success, value};
        if (txn.value % descriptor->alignment != 0)
          return {RegisterStatus::ReservedBits, value};
        value = txn.value; return {RegisterStatus::Success, value};
      }
      if (descriptor->owner == RegisterOwner::PerChannel &&
          descriptor->access == RegisterAccess::ReadWrite) {
        // These product-controlled registers are represented transactionally;
        // field masks remain profile/spec-vector responsibilities.
        auto& value = image.per_axi[0][txn.offset];
        if (!txn.write) return {RegisterStatus::Success, value};
        value = txn.value; return {RegisterStatus::Success, value};
      }
      return {RegisterStatus::UnknownOffset, 0};
  }
}

void RegisterPort::set_ready(address::ChannelId channel, bool ready_value) {
  if (channel.value() < channels_.size()) channels_[channel.value()].ready = ready_value;
}
bool RegisterPort::enabled(address::ChannelId channel) const noexcept {
  return channel.value() < channels_.size() && channels_[channel.value()].enabled;
}
bool RegisterPort::ready(address::ChannelId channel) const noexcept {
  return channel.value() < channels_.size() && channels_[channel.value()].ready;
}
namespace {
bool in_window(std::uint64_t address, std::uint32_t size,
               std::uint64_t start, std::uint64_t units,
               std::uint64_t unit_bytes) noexcept {
  if (size == 0 || units > (~std::uint64_t{0}) / unit_bytes) return false;
  const auto bytes = units * unit_bytes;
  if (start > ~std::uint64_t{0} - bytes || address < start) return false;
  const auto end = start + bytes;
  return address <= end && size <= end - address;
}
}
bool RegisterPort::allows_flash(address::ChannelId channel, std::uint8_t axi,
                                std::uint64_t address, std::uint32_t size) const noexcept {
  if (channel.value() >= channels_.size() || axi >= axi_interfaces_) return false;
  const auto& values = channels_[channel.value()].per_axi[axi];
  return in_window(address, size, values.at(0x0034), values.at(0x003C), 4096);
}
bool RegisterPort::allows_scratchpad(address::ChannelId channel, std::uint8_t axi,
                                     std::uint64_t address, std::uint32_t size) const noexcept {
  if (channel.value() >= channels_.size() || axi >= axi_interfaces_) return false;
  const auto& values = channels_[channel.value()].per_axi[axi];
  return in_window(address, size, values.at(0x001C), values.at(0x0024), 64);
}
void RegisterPort::reset_hbf(address::ChannelId channel) {
  if (channel.value() >= channels_.size()) return;
  auto& image = channels_[channel.value()];
  reset_image(image);
  // TMON.CES is sticky across HBF_RESET.
}
void RegisterPort::set_critical_error(address::ChannelId channel) {
  if (channel.value() < channels_.size()) channels_[channel.value()].critical_error = true;
}

}  // namespace openhbx::hbf::host
