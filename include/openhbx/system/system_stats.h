#pragma once

#include <cstddef>
#include <cstdint>

namespace openhbx {

struct OpenHbxSystemStats {
  std::uint64_t attempts{0};
  std::uint64_t accepted{0};
  std::uint64_t busy{0};
  std::uint64_t rejected{0};
  std::uint64_t callbacks{0};
  std::uint64_t callback_errors{0};
  std::uint64_t resets{0};
  std::uint64_t ticks{0};
  std::size_t outstanding{0};
  std::uint64_t read_accepted_bytes{0};
  std::uint64_t write_accepted_bytes{0};
  std::uint64_t read_completed_requests{0};
  std::uint64_t read_completed_bytes{0};
  std::uint64_t write_completed_requests{0};
  std::uint64_t write_completed_bytes{0};
  std::uint64_t failed_requests{0};
  std::uint64_t latency_samples{0};
  std::uint64_t latency_sum_cycles{0};
  std::uint64_t latency_min_cycles{0};
  std::uint64_t latency_max_cycles{0};
  std::uint64_t first_completion_cycle{0};
  std::uint64_t last_completion_cycle{0};
};

}  // namespace openhbx
