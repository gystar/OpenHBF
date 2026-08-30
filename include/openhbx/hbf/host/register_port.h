#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "openhbx/hbf/address/hbf_address_types.h"

namespace openhbx::hbf::host {

enum class RegisterAccess { ReadOnly, ReadWrite, WriteOneToClear, WriteTrigger };
enum class ResetDomain { PowerOn, HbfReset, Sticky };
enum class RegisterOwner { PerChannel, PerAxi };
struct RegisterDescriptor {
  std::uint16_t offset;
  RegisterAccess access;
  ResetDomain reset_domain;
  RegisterOwner owner;
  std::uint64_t alignment;
};
std::optional<RegisterDescriptor> register_descriptor(std::uint16_t offset) noexcept;
enum class RegisterStatus { Success, UnknownOffset, ReadOnly, ReservedBits, WrongOwner };
struct RegisterTxn {
  address::ChannelId channel;
  std::uint8_t axi_interface{0};
  std::uint16_t offset{0};
  bool write{false};
  std::uint64_t value{0};
};
struct RegisterResult { RegisterStatus status{RegisterStatus::UnknownOffset}; std::uint64_t value{0}; };
using ResetIntentSink = std::function<void(address::ChannelId)>;

class RegisterPort {
 public:
  RegisterPort(std::uint8_t channels, std::uint8_t axi_interfaces,
               std::uint64_t axi_capacity_bytes, ResetIntentSink reset_intent);
  RegisterResult access(const RegisterTxn& transaction);
  void set_ready(address::ChannelId channel, bool ready);
  bool enabled(address::ChannelId channel) const noexcept;
  bool ready(address::ChannelId channel) const noexcept;
  bool allows_flash(address::ChannelId channel, std::uint8_t axi_interface,
                    std::uint64_t address_bytes, std::uint32_t size_bytes) const noexcept;
  bool allows_scratchpad(address::ChannelId channel, std::uint8_t axi_interface,
                         std::uint64_t address_bytes, std::uint32_t size_bytes) const noexcept;
  void reset_hbf(address::ChannelId channel);
  void set_critical_error(address::ChannelId channel);

 private:
  struct ChannelImage {
    bool enabled{false};
    bool ready{false};
    bool critical_error{false};
    std::vector<std::map<std::uint16_t, std::uint64_t>> per_axi;
  };
  std::uint8_t axi_interfaces_;
  std::uint64_t axi_capacity_bytes_;
  ResetIntentSink reset_intent_;
  std::vector<ChannelImage> channels_;
  void reset_image(ChannelImage& image);
};

}  // namespace openhbx::hbf::host
