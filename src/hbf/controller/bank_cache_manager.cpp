#include "openhbx/hbf/controller/bank_cache_manager.h"

#include <stdexcept>

namespace openhbx::hbf::controller {
BankCacheManager::BankCacheManager(std::vector<address::PhysicalBank> banks,
                                   std::size_t buffers_per_bank)
    : buffers_per_bank_(buffers_per_bank) {
  if (buffers_per_bank < 2) throw std::invalid_argument("each bank requires at least two buffers");
  for (auto bank : banks) banks_.emplace(bank, std::vector<Entry>(buffers_per_bank));
}
std::optional<CacheHandle> BankCacheManager::reserve(address::PhysicalBank bank, DluKey key) {
  auto it = banks_.find(bank); if (it == banks_.end()) return std::nullopt;
  for (std::size_t i = 0; i < it->second.size(); ++i) {
    auto& entry = it->second[i];
    if (entry.state == State::Free) {
      entry.state = State::Reserved; entry.key = key; ++entry.epoch; entry.age = next_age_++;
      return CacheHandle{bank, i, entry.epoch};
    }
  }
  std::size_t victim = it->second.size();
  for (std::size_t i = 0; i < it->second.size(); ++i)
    if (it->second[i].state == State::Valid &&
        (victim == it->second.size() || it->second[i].age < it->second[victim].age))
      victim = i;
  if (victim == it->second.size()) return std::nullopt;
  auto& entry = it->second[victim];
  entry.state = State::Reserved; entry.key = key; entry.payload.reset();
  ++entry.epoch; entry.age = next_age_++;
  return CacheHandle{bank, victim, entry.epoch};
}
bool BankCacheManager::fill(const CacheHandle& handle, PayloadHandle payload) {
  auto it = banks_.find(handle.bank);
  if (it == banks_.end() || handle.slot >= it->second.size() || payload.size() != 4096) return false;
  auto& entry = it->second[handle.slot];
  if (entry.state != State::Reserved || entry.epoch != handle.epoch) return false;
  entry.payload = std::move(payload); entry.state = State::Valid; return true;
}
std::optional<CacheHit> BankCacheManager::lookup(address::PhysicalBank bank, const DluKey& key) const {
  const auto it = banks_.find(bank); if (it == banks_.end()) return std::nullopt;
  for (std::size_t i = 0; i < it->second.size(); ++i) {
    const auto& entry = it->second[i];
    if (entry.state == State::Valid && entry.key && !(key < *entry.key) && !(*entry.key < key))
      return CacheHit{{bank, i, entry.epoch}, entry.payload};
  }
  return std::nullopt;
}
bool BankCacheManager::release(const CacheHandle& handle) {
  auto it = banks_.find(handle.bank);
  if (it == banks_.end() || handle.slot >= it->second.size()) return false;
  auto& entry = it->second[handle.slot];
  if (entry.state == State::Free || entry.epoch != handle.epoch) return false;
  entry.state = State::Free; entry.key.reset(); entry.payload.reset(); return true;
}
std::size_t BankCacheManager::free_count(address::PhysicalBank bank) const {
  const auto it = banks_.find(bank); if (it == banks_.end()) return 0;
  std::size_t count = 0; for (const auto& e : it->second) if (e.state == State::Free) ++count; return count;
}
void BankCacheManager::reset() {
  for (auto& pair : banks_) for (auto& entry : pair.second) {
    entry.state = State::Free; entry.key.reset(); entry.payload.reset(); ++entry.epoch; entry.age = 0;
  }
}
}  // namespace openhbx::hbf::controller
