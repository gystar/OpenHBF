#include "openhbx/ras/bad_block_table.h"
namespace openhbx::ras {
bool BadBlockTable::mark(std::uint64_t block, BadBlockReason reason) {
  const auto inserted = entries_.emplace(block, reason).second;
  if (inserted) ++version_;
  return inserted;
}
bool BadBlockTable::is_bad(std::uint64_t block) const { return entries_.count(block) != 0; }
}  // namespace openhbx::ras
