#include "openhbx/system/clock_domain.h"

#include <limits>
#include <stdexcept>

#include "openhbx/common/checked_math.h"

namespace openhbx {

ClockDomain::ClockDomain(std::uint64_t numerator, std::uint64_t denominator,
                         std::uint64_t max_ticks_per_cycle)
    : numerator_(numerator), denominator_(denominator), max_ticks_(max_ticks_per_cycle) {
  if (numerator == 0 || denominator == 0 || max_ticks_per_cycle == 0)
    throw std::invalid_argument("clock ratio and tick ceiling must be positive");
}

std::uint64_t ClockDomain::ticks_due() {
  auto sum = checked_add(accumulator_, numerator_);
  if (!sum) throw std::overflow_error("clock accumulator overflow");
  const std::uint64_t ticks = *sum / denominator_;
  if (ticks > max_ticks_) throw std::overflow_error("clock ticks exceed per-cycle ceiling");
  accumulator_ = *sum % denominator_;
  return ticks;
}

}  // namespace openhbx
