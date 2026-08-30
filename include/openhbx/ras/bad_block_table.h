#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace openhbx::ras {
enum class BadBlockReason { Factory, ProgramFailure, EraseFailure, Retired };
class BadBlockTable {
 public:
  bool mark(std::uint64_t block, BadBlockReason reason);
  bool is_bad(std::uint64_t block) const;
  std::size_t size() const noexcept { return entries_.size(); }
  std::uint64_t version() const noexcept { return version_; }
 private:
  std::map<std::uint64_t, BadBlockReason> entries_;
  std::uint64_t version_{0};
};
}  // namespace openhbx::ras
