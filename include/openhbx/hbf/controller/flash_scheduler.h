#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "openhbx/hbf/controller/controller_types.h"

namespace openhbx::hbf::controller {
enum class WorkKind { Read, Program, Admin };
struct FlashWork { Token token; address::PhysicalBank bank; WorkKind kind; ReadMode mode; std::uint64_t sequence{0}; };

class FlashScheduler {
 public:
  explicit FlashScheduler(std::size_t depth) : depth_(depth) {}
  bool enqueue(FlashWork work);
  std::optional<FlashWork> select();
  void complete(address::PhysicalBank bank);
  bool cancel(Token token);
  std::size_t queued() const noexcept;
  void reset();
 private:
  struct BankQueue { std::vector<FlashWork> work; bool active{false}; std::uint64_t batch_epoch{0}; };
  std::size_t depth_;
  std::uint64_t next_sequence_{0};
  std::map<address::PhysicalBank, BankQueue> banks_;
};
}  // namespace openhbx::hbf::controller
