#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "openhbf/common/error.h"
#include "openhbf/media/topology.h"

namespace openhbf::media {

struct PendingPayloadTag;
using PendingPayload = StrongValue<PendingPayloadTag, std::uint64_t>;

class IPageStore {
 public:
  virtual ~IPageStore() = default;
  virtual Result<PendingPayload> stage_program(PageKey key,
                                                SharedPage payload) = 0;
  virtual Result<void> commit_program(PendingPayload pending) = 0;
  virtual Result<void> abort_program(PendingPayload pending) = 0;
  virtual Result<PayloadSnapshot> snapshot(PageKey key,
                                            const MediaAddress& address,
                                            std::uint64_t block_epoch) const = 0;
  virtual void erase_block(const ResolvedBlock& block) = 0;
  virtual std::size_t allocated_bytes() const noexcept = 0;
};

std::unique_ptr<IPageStore> make_page_store(PayloadMode mode);

}  // namespace openhbf::media
