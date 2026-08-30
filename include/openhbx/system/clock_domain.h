#pragma once

#include <cstdint>

namespace openhbx {

class ClockDomain {
 public:
  ClockDomain(std::uint64_t numerator, std::uint64_t denominator,
              std::uint64_t max_ticks_per_cycle = 1024);
  std::uint64_t ticks_due();
  std::uint64_t accumulator() const noexcept { return accumulator_; }
 private:
  std::uint64_t numerator_, denominator_, max_ticks_, accumulator_{0};
};

}  // namespace openhbx
