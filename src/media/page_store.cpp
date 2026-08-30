#include "openhbf/media/page_store.h"

#include <utility>

namespace openhbf::media {
namespace {

std::uint64_t signature(const PageData& data) noexcept {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto byte : data) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return hash;
}

struct StoredPayload {
  SharedPage bytes;
  std::uint64_t signature = 0;
};
struct StagedPayload {
  PageKey key = 0;
  StoredPayload payload;
};

class PageStore final : public IPageStore {
 public:
  explicit PageStore(PayloadMode mode) : mode_(mode) {}

  Result<PendingPayload> stage_program(PageKey key,
                                        SharedPage payload) override {
    if (!payload) {
      return Result<PendingPayload>::failure(
          {ErrorCode::kInvalidArgument, "program payload must contain 4096 bytes"});
    }
    const PendingPayload handle(next_pending_++);
    // Functional mode retains the immutable shared buffer. Timing mode drops
    // it after computing the deterministic signature.
    StagedPayload staged{key, {mode_ == PayloadMode::FunctionalSparse ? payload
                                                                      : SharedPage{},
                                signature(*payload)}};
    pending_.emplace(handle.value(), std::move(staged));
    return Result<PendingPayload>::success(handle);
  }

  Result<void> commit_program(PendingPayload handle) override {
    const auto it = pending_.find(handle.value());
    if (it == pending_.end()) {
      return Result<void>::failure(
          {ErrorCode::kIntegrity, "unknown pending payload"});
    }
    committed_[it->second.key] = std::move(it->second.payload);
    pending_.erase(it);
    return Result<void>::success();
  }

  Result<void> abort_program(PendingPayload handle) override {
    if (pending_.erase(handle.value()) != 1) {
      return Result<void>::failure(
          {ErrorCode::kIntegrity, "unknown pending payload"});
    }
    return Result<void>::success();
  }

  Result<PayloadSnapshot> snapshot(PageKey key, const MediaAddress& address,
                                   std::uint64_t block_epoch) const override {
    const auto it = committed_.find(key);
    if (it == committed_.end()) {
      return Result<PayloadSnapshot>::failure(
          {ErrorCode::kOutOfRange, "page has no committed payload"});
    }
    return Result<PayloadSnapshot>::success(
        {address, block_epoch, mode_ == PayloadMode::FunctionalSparse,
         it->second.signature, it->second.bytes});
  }

  void erase_block(const ResolvedBlock& block) override {
    const auto end = block.first_page + block.page_count;
    for (PageKey key = block.first_page; key < end; ++key) committed_.erase(key);
  }

  std::size_t allocated_bytes() const noexcept override {
    if (mode_ == PayloadMode::TimingOnly) return 0;
    return committed_.size() * kPageBytes;
  }

 private:
  PayloadMode mode_;
  std::uint64_t next_pending_ = 1;
  std::unordered_map<std::uint64_t, StagedPayload> pending_;
  std::unordered_map<PageKey, StoredPayload> committed_;
};

}  // namespace

std::unique_ptr<IPageStore> make_page_store(PayloadMode mode) {
  return std::unique_ptr<IPageStore>(new PageStore(mode));
}

}  // namespace openhbf::media
