#pragma once

#include <cstdint>

#include "openhbf/integration/open_hbf_system.h"

namespace openhbf::integration {

struct RunnerLimits {
  std::uint64_t submit_retry_cycles = 1000000;
  std::uint64_t drain_cycles = 1000000;
};

struct RunnerSubmitResult {
  bool accepted = false;
  RequestToken token{};
  std::uint64_t retry_cycles = 0;
  SystemSnapshot snapshot;
};

class DirectRunner final {
 public:
  explicit DirectRunner(OpenHbfSystem& system, RunnerLimits limits = {})
      : system_(system), limits_(limits) {}

  RunnerSubmitResult submit(HostRequest request, CompletionSink completion);
  DrainResult drain();
  void tick() { system_.tick(); }

 private:
  OpenHbfSystem& system_;
  RunnerLimits limits_;
};

}  // namespace openhbf::integration
