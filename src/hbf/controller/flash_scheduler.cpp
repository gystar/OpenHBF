#include "openhbx/hbf/controller/flash_scheduler.h"

#include <algorithm>

namespace openhbx::hbf::controller {
bool FlashScheduler::enqueue(FlashWork work) {
  auto& queue = banks_[work.bank];
  if (queue.work.size() >= depth_) return false;
  work.sequence = next_sequence_++;
  if (work.mode == ReadMode::Regular) ++queue.batch_epoch;
  queue.work.push_back(work);
  return true;
}
std::optional<FlashWork> FlashScheduler::select() {
  BankQueue* selected = nullptr;
  for (auto& pair : banks_) {
    auto& queue = pair.second;
    if (queue.active || queue.work.empty()) continue;
    // Sense order within one Bank is strict FIFO. A Regular request closes
    // the preceding Batch generation at enqueue time but never overtakes it.
    if (selected == nullptr || queue.work.front().sequence < selected->work.front().sequence)
      selected = &queue;
  }
  if (selected == nullptr) return std::nullopt;
  auto result = selected->work.front();
  selected->work.erase(selected->work.begin());
  selected->active = true;
  return result;
}
void FlashScheduler::complete(address::PhysicalBank bank) { banks_[bank].active = false; }
bool FlashScheduler::cancel(Token token) {
  for (auto& pair : banks_) {
    auto& work = pair.second.work;
    const auto it = std::find_if(work.begin(), work.end(),
        [token](const FlashWork& item) { return item.token == token; });
    if (it != work.end()) { work.erase(it); return true; }
  }
  return false;
}
std::size_t FlashScheduler::queued() const noexcept {
  std::size_t count = 0; for (const auto& pair : banks_) count += pair.second.work.size(); return count;
}
void FlashScheduler::reset() { banks_.clear(); next_sequence_ = 0; }
}  // namespace openhbx::hbf::controller
