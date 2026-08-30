#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "openhbf/integration/host_request.h"

namespace openhbf::integration {

enum class CompletionState { Accepted, Issued };
enum class FinishResult { Delivered, CallbackFailed };
using CallbackErrorSink = std::function<void(RequestToken, const std::string&)>;

struct CompletionSnapshot {
  std::size_t accepted = 0;
  std::size_t issued = 0;
  std::size_t outstanding = 0;
  std::size_t terminal = 0;
  std::size_t callback_failures = 0;
};

class CompletionRegistry {
 public:
  explicit CompletionRegistry(CallbackErrorSink error_sink = {});

  RequestToken register_request(CompletionSink sink, Generation generation,
                                Cycle accepted_cycle);
  Result<void> mark_issued(RequestToken token, Cycle cycle);
  Result<FinishResult> finish_once(Completion completion);
  std::size_t abort_generation(Generation generation, HostStatus status,
                               Cycle complete_cycle);
  bool empty() const noexcept { return entries_.empty(); }
  CompletionSnapshot snapshot() const noexcept;

 private:
  struct Entry {
    CompletionSink sink;
    Generation generation{};
    CompletionState state = CompletionState::Accepted;
    Cycle accepted_cycle{};
    std::optional<Cycle> issue_cycle;
  };

  std::unordered_map<RequestToken, Entry> entries_;
  std::uint64_t next_token_ = 1;
  std::size_t accepted_ = 0;
  std::size_t terminal_ = 0;
  std::size_t callback_failures_ = 0;
  CallbackErrorSink error_sink_;

  FinishResult dispatch(RequestToken token, CompletionSink sink,
                        const Completion& completion) noexcept;
};

}  // namespace openhbf::integration
