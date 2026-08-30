#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include "openhbf/common/error.h"
#include "openhbf/common/types.h"
#include "openhbf/media/types.h"

namespace openhbf::ftl {

constexpr std::size_t kDluBytes = 64;
struct ByteAddressTag;
struct Unit64Tag;
struct DluTag;
using ByteAddress = StrongValue<ByteAddressTag, std::uint64_t>;
using Unit64 = StrongValue<Unit64Tag, std::uint64_t>;
using Dlu = StrongValue<DluTag, std::uint64_t>;
using HostToken = RequestToken;

using Payload4KiB = std::array<std::uint8_t, 4096>;

struct ProgramDluRequest {
  HostToken token{};
  ChannelId channel{};
  Dlu address{};
  std::shared_ptr<const Payload4KiB> payload;
};
struct ReadDluRequest { HostToken token{}; ChannelId channel{}; Dlu address{}; };

enum class SequenceMode : std::uint8_t { Normal, ReplayRequired };
struct BlockKey { ChannelId channel{}; std::uint16_t core_die=0, die=0, bank=0; std::uint32_t block=0;
  friend bool operator==(const BlockKey&a,const BlockKey&b) noexcept { return a.channel==b.channel&&a.core_die==b.core_die&&a.die==b.die&&a.bank==b.bank&&a.block==b.block; }
};
struct PhysicalDluAddress { ChannelId owner_channel{}; std::uint16_t core_die=0, die=0, bank=0; std::uint32_t block=0, page=0; };
struct SequenceReservation { FtlToken token{}; Generation generation{}; BlockKey block{}; std::uint32_t page=0; };
struct BlockSequenceState { SequenceMode mode=SequenceMode::Normal; std::optional<std::uint32_t> failed_page; std::uint32_t replay_progress=0; std::optional<SequenceReservation> reservation; };

} // namespace openhbf::ftl

namespace std { template<> struct hash<openhbf::ftl::BlockKey> { size_t operator()(const openhbf::ftl::BlockKey&k) const noexcept { size_t h=k.channel.value(); h=h*31+k.core_die; h=h*31+k.die; h=h*31+k.bank; return h*31+k.block; } }; }
