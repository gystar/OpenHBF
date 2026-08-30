#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "openhbx/hbf/controller/controller_types.h"

namespace openhbx::hbf::controller {
struct CacheHandle { address::PhysicalBank bank; std::size_t slot{0}; std::uint64_t epoch{0}; };
struct CacheHit { CacheHandle handle; PayloadHandle payload; };
class BankCacheManager {
 public:
  BankCacheManager(std::vector<address::PhysicalBank> banks, std::size_t buffers_per_bank);
  std::optional<CacheHandle> reserve(address::PhysicalBank bank, DluKey key);
  bool fill(const CacheHandle& handle, PayloadHandle payload);
  std::optional<CacheHit> lookup(address::PhysicalBank bank, const DluKey& key) const;
  bool release(const CacheHandle& handle);
  std::size_t free_count(address::PhysicalBank bank) const;
  void reset();
 private:
  enum class State { Free, Reserved, Valid };
  struct Entry { State state{State::Free}; std::optional<DluKey> key; PayloadHandle payload; std::uint64_t epoch{0}; std::uint64_t age{0}; };
  std::size_t buffers_per_bank_;
  std::uint64_t next_age_{1};
  std::map<address::PhysicalBank, std::vector<Entry>> banks_;
};
}  // namespace openhbx::hbf::controller
