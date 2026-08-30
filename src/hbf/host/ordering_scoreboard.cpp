#include "openhbx/hbf/host/ordering_scoreboard.h"

#include <algorithm>

namespace openhbx::hbf::host {

bool OrderingScoreboard::reserve(Token token, OrderingKey key,
                                 std::uint64_t address_bytes,
                                 std::uint32_t size_bytes) {
  if (token.value() == 0 || records_.count(token.value()) != 0 ||
      address_bytes % 64 != 0 || size_bytes == 0 || size_bytes % 64 != 0) return false;
  Record record; record.key = key;
  for (std::uint64_t offset = 0; offset < size_bytes; offset += 64)
    record.addresses.push_back({key.channel, key.axi_interface,
                                (address_bytes + offset) / 64});
  records_.emplace(token.value(), record);
  id_queues_[key].push_back(token);
  for (const auto& address : record.addresses) address_queues_[address].push_back(token);
  return true;
}

bool OrderingScoreboard::cancel(Token token) {
  const auto it = records_.find(token.value());
  if (it == records_.end()) return false;
  erase(token, it->second); records_.erase(it); return true;
}

bool OrderingScoreboard::mark_ready(Token token) {
  const auto it = records_.find(token.value());
  if (it == records_.end() || it->second.ready) return false;
  it->second.ready = true; return true;
}

bool OrderingScoreboard::releasable(Token token, const Record& record) const {
  const auto id = id_queues_.find(record.key);
  if (id == id_queues_.end() || id->second.empty() || id->second.front() != token) return false;
  for (const auto& address : record.addresses) {
    const auto queue = address_queues_.find(address);
    if (queue == address_queues_.end() || queue->second.empty() || queue->second.front() != token)
      return false;
  }
  return true;
}

void OrderingScoreboard::erase(Token token, const Record& record) {
  auto& ids = id_queues_[record.key];
  ids.erase(std::find(ids.begin(), ids.end(), token));
  if (ids.empty()) id_queues_.erase(record.key);
  for (const auto& address : record.addresses) {
    auto& addresses = address_queues_[address];
    addresses.erase(std::find(addresses.begin(), addresses.end(), token));
    if (addresses.empty()) address_queues_.erase(address);
  }
}

std::vector<Token> OrderingScoreboard::release_ready() {
  std::vector<Token> released;
  bool progress = true;
  while (progress) {
    progress = false;
    for (auto it = records_.begin(); it != records_.end(); ++it) {
      const Token token(it->first);
      if (!it->second.ready || !releasable(token, it->second)) continue;
      const auto record = it->second;
      erase(token, record); records_.erase(it); released.push_back(token);
      progress = true; break;
    }
  }
  return released;
}

void OrderingScoreboard::clear() noexcept {
  records_.clear(); id_queues_.clear(); address_queues_.clear();
}

}  // namespace openhbx::hbf::host
