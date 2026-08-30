#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "openhbf/common/error.h"
#include "openhbf/common/types.h"

namespace openhbf {

namespace detail {

template <typename T>
using EnableUnsigned =
    typename std::enable_if<std::is_integral<T>::value &&
                                std::is_unsigned<T>::value,
                            int>::type;

inline Error arithmetic_error(ErrorCode code, const char* operation,
                              std::uint64_t lhs, std::uint64_t rhs) {
  return Error{code, std::string(operation) + " failed for lhs=" +
                         std::to_string(lhs) + ", rhs=" +
                         std::to_string(rhs)};
}

}  // namespace detail

// Integral helpers return Result instead of wrapping.  This is important for
// topology products and event deadlines, where wraparound can corrupt owners.
template <typename T, detail::EnableUnsigned<T> = 0>
Result<T> checked_add(T lhs, T rhs) {
  if (rhs > std::numeric_limits<T>::max() - lhs) {
    return Result<T>::failure(detail::arithmetic_error(
        ErrorCode::kOverflow, "checked_add", lhs, rhs));
  }
  return Result<T>::success(static_cast<T>(lhs + rhs));
}

template <typename T, detail::EnableUnsigned<T> = 0>
Result<T> checked_sub(T lhs, T rhs) {
  if (rhs > lhs) {
    return Result<T>::failure(detail::arithmetic_error(
        ErrorCode::kUnderflow, "checked_sub", lhs, rhs));
  }
  return Result<T>::success(static_cast<T>(lhs - rhs));
}

template <typename T, detail::EnableUnsigned<T> = 0>
Result<T> checked_mul(T lhs, T rhs) {
  if (lhs != 0 && rhs > std::numeric_limits<T>::max() / lhs) {
    return Result<T>::failure(detail::arithmetic_error(
        ErrorCode::kOverflow, "checked_mul", lhs, rhs));
  }
  return Result<T>::success(static_cast<T>(lhs * rhs));
}

template <typename T, detail::EnableUnsigned<T> = 0>
Result<T> checked_ceil_div(T numerator, T denominator) {
  if (denominator == 0) {
    return Result<T>::failure(detail::arithmetic_error(
        ErrorCode::kDivisionByZero, "checked_ceil_div", numerator,
        denominator));
  }
  // This formulation avoids the overflow in (numerator + denominator - 1).
  return Result<T>::success(static_cast<T>(
      numerator / denominator + (numerator % denominator != 0 ? 1 : 0)));
}

inline Result<Cycle> checked_add(Cycle cycle, Duration duration) {
  auto result = checked_add(cycle.value(), duration.value());
  if (!result) {
    return Result<Cycle>::failure(result.error());
  }
  return Result<Cycle>::success(Cycle(result.value()));
}

inline Result<Duration> checked_add(Duration lhs, Duration rhs) {
  auto result = checked_add(lhs.value(), rhs.value());
  if (!result) {
    return Result<Duration>::failure(result.error());
  }
  return Result<Duration>::success(Duration(result.value()));
}

inline Result<Duration> checked_sub(Cycle lhs, Cycle rhs) {
  auto result = checked_sub(lhs.value(), rhs.value());
  if (!result) {
    return Result<Duration>::failure(result.error());
  }
  return Result<Duration>::success(Duration(result.value()));
}

inline Result<Duration> checked_sub(Duration lhs, Duration rhs) {
  auto result = checked_sub(lhs.value(), rhs.value());
  if (!result) {
    return Result<Duration>::failure(result.error());
  }
  return Result<Duration>::success(Duration(result.value()));
}

inline Result<Duration> checked_mul(Duration duration, std::uint64_t factor) {
  auto result = checked_mul(duration.value(), factor);
  if (!result) {
    return Result<Duration>::failure(result.error());
  }
  return Result<Duration>::success(Duration(result.value()));
}

inline Result<Duration> checked_ceil_div(Duration duration,
                                         std::uint64_t divisor) {
  auto result = checked_ceil_div(duration.value(), divisor);
  if (!result) {
    return Result<Duration>::failure(result.error());
  }
  return Result<Duration>::success(Duration(result.value()));
}

}  // namespace openhbf
