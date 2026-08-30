#pragma once

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "openhbx/hbf/host/host_types.h"

namespace openhbx::hbf::host {

struct OrderingKey {
  address::ChannelId channel;
  std::uint8_t axi_interface{0};
  AxiId axi_id;
  friend bool operator<(const OrderingKey& a, const OrderingKey& b) noexcept {
    if (a.channel != b.channel) return a.channel < b.channel;
    if (a.axi_interface != b.axi_interface) return a.axi_interface < b.axi_interface;
    return a.axi_id < b.axi_id;
  }
};

class OrderingScoreboard {
 public:
  bool reserve(Token token, OrderingKey key, std::uint64_t address_bytes,
               std::uint32_t size_bytes);
  bool cancel(Token token);
  bool mark_ready(Token token);
  std::vector<Token> release_ready();
  std::size_t outstanding() const noexcept { return records_.size(); }
  void clear() noexcept;

 private:
  struct AddressKey {
    address::ChannelId channel;
    std::uint8_t axi_interface{0};
    std::uint64_t address64{0};
    friend bool operator<(const AddressKey& a, const AddressKey& b) noexcept {
      if (a.channel != b.channel) return a.channel < b.channel;
      if (a.axi_interface != b.axi_interface) return a.axi_interface < b.axi_interface;
      return a.address64 < b.address64;
    }
  };
  struct Record { OrderingKey key; std::vector<AddressKey> addresses; bool ready{false}; };
  bool releasable(Token token, const Record& record) const;
  void erase(Token token, const Record& record);
  std::map<std::uint64_t, Record> records_;
  std::map<OrderingKey, std::deque<Token>> id_queues_;
  std::map<AddressKey, std::deque<Token>> address_queues_;
};

}  // namespace openhbx::hbf::host
