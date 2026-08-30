#include "openhbf/integration/direct_runner.h"

#include <utility>

namespace openhbf::integration {

RunnerSubmitResult DirectRunner::submit(HostRequest request,
                                        CompletionSink completion) {
  std::uint64_t retries = 0;
  auto outcome = system_.try_submit(request, completion);
  while (outcome.result == SubmitResult::Retry) {
    if (retries == limits_.submit_retry_cycles)
      return {false, RequestToken{}, retries, system_.snapshot()};
    system_.tick();
    ++retries;
    outcome = system_.try_submit(request, completion);
  }
  return {true, outcome.token, retries, system_.snapshot()};
}

DrainResult DirectRunner::drain() { return system_.drain(limits_.drain_cycles); }

}  // namespace openhbf::integration
