#include "openhbx/system/completion_registry.h"

#include <utility>

namespace openhbx {

AdmissionResult CompletionRegistry::register_request(Token token, Generation generation, Sink sink) {
  if (token.value() == 0 || generation.value() == 0 || !sink)
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  if (entries_.count(token.value()) || retired_.count(token.value()))
    return AdmissionResult::rejected(RejectionReason::InvalidArgument, "token is not unique");
  if (entries_.size() >= capacity_) return AdmissionResult::busy();
  entries_.emplace(token.value(), Entry{generation, ObligationState::Accepted, std::move(sink), std::nullopt});
  ++accepted_;
  return AdmissionResult::accepted();
}

bool CompletionRegistry::mark_issued(Token token, Generation generation) {
  auto it = entries_.find(token.value());
  if (it == entries_.end() || it->second.generation != generation ||
      it->second.state != ObligationState::Accepted) return false;
  it->second.state = ObligationState::Issued;
  ++issued_;
  return true;
}

FinishCode CompletionRegistry::finish_once(Completion completion) {
  const Token token = completion.token;
  const auto staged = stage_terminal(std::move(completion));
  return staged == FinishCode::Delivered ? deliver_once(token) : staged;
}

FinishCode CompletionRegistry::stage_terminal(Completion completion) {
  auto it = entries_.find(completion.token.value());
  if (it == entries_.end())
    return retired_.count(completion.token.value()) ? FinishCode::DuplicateToken : FinishCode::UnknownToken;
  if (it->second.generation != completion.generation) return FinishCode::StaleGeneration;
  if (it->second.state == ObligationState::Delivering) return FinishCode::DuplicateToken;
  it->second.state = ObligationState::Delivering;
  it->second.terminal = std::move(completion);
  return FinishCode::Delivered;
}

FinishCode CompletionRegistry::deliver_once(Token token) {
  auto it = entries_.find(token.value());
  if (it == entries_.end())
    return retired_.count(token.value()) ? FinishCode::DuplicateToken : FinishCode::UnknownToken;
  if (it->second.state != ObligationState::Delivering || !it->second.terminal)
    return FinishCode::UnknownToken;
  Sink sink = std::move(it->second.sink);
  Completion completion = std::move(*it->second.terminal);
  const Generation generation = completion.generation;
  entries_.erase(it);
  retired_.emplace(token.value(), generation);
  ++terminal_;
  sink(std::move(completion));
  return FinishCode::Delivered;
}

std::vector<Token> CompletionRegistry::stage_abort_generation(Generation old_generation) {
  std::vector<Token> tokens;
  for (const auto& pair : entries_)
    if (pair.second.generation == old_generation &&
        pair.second.state != ObligationState::Delivering)
      tokens.emplace_back(pair.first);
  for (Token token : tokens)
    stage_terminal({token, old_generation, TerminalCode::AbortedByReset, false, {}});
  return tokens;
}

std::size_t CompletionRegistry::abort_generation(Generation old_generation) {
  auto tokens = stage_abort_generation(old_generation);
  for (Token token : tokens)
    deliver_once(token);
  return tokens.size();
}

CompletionSnapshot CompletionRegistry::snapshot() const {
  std::vector<Token> tokens;
  for (const auto& pair : entries_) tokens.emplace_back(pair.first);
  return {accepted_, issued_, terminal_, entries_.size(), std::move(tokens)};
}

}  // namespace openhbx
