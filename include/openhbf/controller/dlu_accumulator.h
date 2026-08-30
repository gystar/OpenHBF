#pragma once
#include <bitset>
#include <unordered_map>
#include <limits>
#include "controller_types.h"
namespace openhbf::controller {
enum class Hazard { Absent, Forward, Pending };
struct HazardResult { Hazard kind; std::array<uint8_t,64> data{}; };
class DluAccumulator {
 public:
  DluAccumulator(uint16_t channels, size_t max_entries, Cycle timeout): channels_(channels), max_entries_(max_entries), timeout_(timeout) {}
  Status accept(const WriteSegment&, Cycle, std::optional<ReadyDlu>* ready);
  HazardResult probe(ChannelId, DluIndex, SegmentIndex) const;
  struct ExpiredDlu { ChannelId channel{}; DluIndex dlu{}; Generation generation{}; std::vector<HostToken> waiters; };
  std::vector<ExpiredDlu> expire(Cycle now);
  // Remove all entries in a generation and return their waiters for abort reporting.
  // Callers that do not need reporting may safely ignore the returned vector.
  std::vector<ExpiredDlu> cancel(Generation);
 private:
  struct Key {
    ChannelId channel{};
    DluIndex dlu{};
    friend bool operator==(const Key& lhs, const Key& rhs) noexcept {
      return lhs.channel == rhs.channel && lhs.dlu == rhs.dlu;
    }
  };
  struct KeyHash {
    size_t operator()(const Key& key) const noexcept {
      const auto channel = static_cast<size_t>(key.channel.value());
      const auto dlu = static_cast<size_t>(key.dlu);
      // Hash both fields independently; no bit-width assumption is made for DLU.
      return (channel * static_cast<size_t>(0x9e3779b9U)) ^
             (dlu + static_cast<size_t>(0x85ebca6bU) + (dlu >> 16U));
    }
  };
  struct Entry { std::bitset<64> mask; std::array<std::array<uint8_t,64>,64> data{}; std::vector<HostToken> waiters; Cycle deadline{}; Generation generation{}; };
  uint16_t channels_; size_t max_entries_; Cycle timeout_;
  std::unordered_map<Key, Entry, KeyHash> entries_;
};
}
