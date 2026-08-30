#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "openhbx/common/admission.h"
#include "openhbx/common/payload_handle.h"
#include "openhbx/common/strong_types.h"

namespace openhbx {

enum class ObligationState { Accepted, Issued, Delivering };
enum class FinishCode { Delivered, UnknownToken, DuplicateToken, StaleGeneration };
enum class TerminalCode { Success, Failed, AbortedByReset };
struct Completion { Token token; Generation generation; TerminalCode code; bool data_valid; PayloadHandle payload; };
struct CompletionSnapshot { std::uint64_t accepted, issued, terminal; std::size_t outstanding; std::vector<Token> tokens; };

class CompletionRegistry {
 public:
  using Sink = std::function<void(Completion)>;
  explicit CompletionRegistry(std::size_t capacity) : capacity_(capacity) {}
  bool can_register(Token token) const noexcept {
    return token.value() != 0 && entries_.size() < capacity_ &&
           entries_.count(token.value()) == 0 && retired_.count(token.value()) == 0;
  }
  AdmissionResult register_request(Token token, Generation generation, Sink sink);
  bool mark_issued(Token token, Generation generation);
  FinishCode stage_terminal(Completion completion);
  FinishCode deliver_once(Token token);
  FinishCode finish_once(Completion completion);
  std::vector<Token> stage_abort_generation(Generation old_generation);
  std::size_t abort_generation(Generation old_generation);
  CompletionSnapshot snapshot() const;
  bool empty() const noexcept { return entries_.empty(); }
 private:
  struct Entry { Generation generation; ObligationState state; Sink sink;
                 std::optional<Completion> terminal; };
  std::size_t capacity_;
  std::map<std::uint64_t, Entry> entries_;
  std::map<std::uint64_t, Generation> retired_;
  std::uint64_t accepted_{0}, issued_{0}, terminal_{0};
};

}  // namespace openhbx
