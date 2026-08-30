#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace openhbf {

// StrongValue prevents accidental comparison or assignment between identifiers
// that happen to use the same integer representation.  Conversion back to the
// representation is deliberately explicit through value().
template <typename Tag, typename Rep>
class StrongValue {
  static_assert(std::is_integral<Rep>::value,
                "StrongValue requires an integral representation");

 public:
  using rep_type = Rep;

  constexpr StrongValue() noexcept = default;
  explicit constexpr StrongValue(Rep value) noexcept : value_(value) {}

  constexpr Rep value() const noexcept { return value_; }

  friend constexpr bool operator==(StrongValue lhs, StrongValue rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }
  friend constexpr bool operator!=(StrongValue lhs, StrongValue rhs) noexcept {
    return !(lhs == rhs);
  }
  friend constexpr bool operator<(StrongValue lhs, StrongValue rhs) noexcept {
    return lhs.value_ < rhs.value_;
  }
  friend constexpr bool operator<=(StrongValue lhs, StrongValue rhs) noexcept {
    return !(rhs < lhs);
  }
  friend constexpr bool operator>(StrongValue lhs, StrongValue rhs) noexcept {
    return rhs < lhs;
  }
  friend constexpr bool operator>=(StrongValue lhs, StrongValue rhs) noexcept {
    return !(lhs < rhs);
  }

 private:
  Rep value_ = 0;
};

struct CycleTag;
struct DurationTag;
struct GenerationTag;
struct ChannelIdTag;
struct RequestTokenTag;
struct CommandTokenTag;
struct FtlTokenTag;
struct EventTokenTag;
struct ResourceIdTag;

using Cycle = StrongValue<CycleTag, std::uint64_t>;
using Duration = StrongValue<DurationTag, std::uint64_t>;
using Generation = StrongValue<GenerationTag, std::uint64_t>;
using ChannelId = StrongValue<ChannelIdTag, std::uint16_t>;
using RequestToken = StrongValue<RequestTokenTag, std::uint64_t>;
using CommandToken = StrongValue<CommandTokenTag, std::uint64_t>;
using FtlToken = StrongValue<FtlTokenTag, std::uint64_t>;
using EventToken = StrongValue<EventTokenTag, std::uint64_t>;
using ResourceId = StrongValue<ResourceIdTag, std::uint64_t>;

// Token zero is never allocated.  Generation and ChannelId intentionally do
// not use this rule: generation zero is the initial epoch and Channel zero is
// a valid OCP owner channel.
template <typename Token>
constexpr bool is_valid_token(Token token) noexcept {
  return token.value() != 0;
}

}  // namespace openhbf

namespace std {

template <typename Tag, typename Rep>
struct hash<openhbf::StrongValue<Tag, Rep>> {
  size_t operator()(openhbf::StrongValue<Tag, Rep> value) const noexcept {
    return hash<Rep>{}(value.value());
  }
};

}  // namespace std
