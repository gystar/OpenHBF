#include "openhbf/integration/completion_registry.h"

#include <limits>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace openhbf::integration {
namespace {
template <typename T>
Result<T> integrity(std::string message) {
  return Result<T>::failure({ErrorCode::kIntegrity, std::move(message)});
}
}  // namespace

CompletionRegistry::CompletionRegistry(CallbackErrorSink error_sink)
    : error_sink_(std::move(error_sink)) {}

RequestToken CompletionRegistry::register_request(CompletionSink sink,
                                                  Generation generation,
                                                  Cycle accepted_cycle) {
  if (!sink) throw std::invalid_argument("completion sink must not be empty");
  if (next_token_ == 0 || next_token_ == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("request token space exhausted");
  const RequestToken token(next_token_++);
  const auto inserted = entries_.emplace(
      token, Entry{std::move(sink), generation, CompletionState::Accepted,
                   accepted_cycle, std::nullopt});
  if (!inserted.second) throw std::logic_error("request token reused");
  ++accepted_;
  return token;
}

Result<void> CompletionRegistry::mark_issued(RequestToken token, Cycle cycle) {
  auto it = entries_.find(token);
  if (it == entries_.end())
    return Result<void>::failure({ErrorCode::kIntegrity,
                                  "unknown or terminal request token=" +
                                      std::to_string(token.value())});
  if (it->second.state == CompletionState::Issued)
    return Result<void>::failure({ErrorCode::kIntegrity,
                                  "request token already issued=" +
                                      std::to_string(token.value())});
  if (cycle < it->second.accepted_cycle)
    return Result<void>::failure({ErrorCode::kIntegrity,
                                  "issue cycle precedes accepted cycle"});
  it->second.state = CompletionState::Issued;
  it->second.issue_cycle = cycle;
  return Result<void>::success();
}

FinishResult CompletionRegistry::dispatch(RequestToken token, CompletionSink sink,
                                          const Completion& completion) noexcept {
  try {
    sink(completion);
    return FinishResult::Delivered;
  } catch (const std::exception& error) {
    ++callback_failures_;
    if (error_sink_) {
      try { error_sink_(token, error.what()); } catch (...) {}
    }
  } catch (...) {
    ++callback_failures_;
    if (error_sink_) {
      try { error_sink_(token, "non-standard callback exception"); } catch (...) {}
    }
  }
  return FinishResult::CallbackFailed;
}

Result<FinishResult> CompletionRegistry::finish_once(Completion completion) {
  auto it = entries_.find(completion.token);
  if (it == entries_.end())
    return integrity<FinishResult>("unknown, duplicate, or late request token=" +
                                   std::to_string(completion.token.value()));
  const Entry& current = it->second;
  if (completion.complete_cycle < current.accepted_cycle ||
      (current.issue_cycle && completion.complete_cycle < *current.issue_cycle))
    return integrity<FinishResult>("completion cycle violates lifecycle ordering");
  Entry entry = std::move(it->second);
  completion.accepted_cycle = entry.accepted_cycle;
  completion.issue_cycle = entry.issue_cycle;
  entries_.erase(it);  // Terminal before callback: callback re-entry cannot repeat it.
  ++terminal_;
  return Result<FinishResult>::success(
      dispatch(completion.token, std::move(entry.sink), completion));
}

std::size_t CompletionRegistry::abort_generation(Generation generation,
                                                 HostStatus status,
                                                 Cycle complete_cycle) {
  struct Pending { RequestToken token; Entry entry; };
  std::vector<Pending> pending;
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.generation != generation) { ++it; continue; }
    pending.push_back({it->first, std::move(it->second)});
    it = entries_.erase(it);
  }
  std::sort(pending.begin(), pending.end(), [](const Pending& lhs,
                                               const Pending& rhs) {
    return lhs.token < rhs.token;
  });
  terminal_ += pending.size();
  for (auto& item : pending) {
    Completion completion{item.token, generation, status, item.entry.accepted_cycle,
                          item.entry.issue_cycle, complete_cycle, {}};
    dispatch(item.token, std::move(item.entry.sink), completion);
  }
  return pending.size();
}

CompletionSnapshot CompletionRegistry::snapshot() const noexcept {
  std::size_t issued = 0;
  for (const auto& item : entries_)
    if (item.second.state == CompletionState::Issued) ++issued;
  return {accepted_, issued, entries_.size(), terminal_, callback_failures_};
}
}  // namespace openhbf::integration
