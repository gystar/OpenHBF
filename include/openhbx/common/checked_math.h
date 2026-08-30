#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace openhbx {

inline std::optional<std::uint64_t> checked_add(std::uint64_t lhs,
                                                std::uint64_t rhs) noexcept {
  if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) return std::nullopt;
  return lhs + rhs;
}

inline std::optional<std::uint64_t> checked_mul(std::uint64_t lhs,
                                                std::uint64_t rhs) noexcept {
  if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) return std::nullopt;
  return lhs * rhs;
}

}  // namespace openhbx
