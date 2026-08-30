#pragma once
#include "openhbf/ftl/types.h"
#include <unordered_map>
namespace openhbf::ftl {
class BlockSequenceTable { public:
  Result<SequenceReservation> reserve(const BlockKey&,std::uint32_t,FtlToken, Generation = Generation(0));
  Result<void> complete(FtlToken, Generation, bool success, std::uint32_t failed_page);
  Result<void> complete(FtlToken token, bool success, std::uint32_t page) { return complete(token, Generation(0), success, page); }
  Result<void> cancel(FtlToken, Generation);
  Result<void> advance_replay(FtlToken, Generation);
  const BlockSequenceState* inspect(const BlockKey&) const noexcept;
 private:
  std::unordered_map<BlockKey, BlockSequenceState> states_;
  std::unordered_map<std::uint64_t, BlockKey> token_blocks_;
};
}
