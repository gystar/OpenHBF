#include <cassert>
#include <cstdint>
#include <vector>

#include "openhbx/hbf/host/host_validator.h"
#include "openhbx/hbf/host/ordering_scoreboard.h"
#include "openhbx/hbf/host/packet_codec.h"
#include "openhbx/hbf/host/register_port.h"

using namespace openhbx;
using namespace openhbx::hbf;
using namespace openhbx::hbf::host;

int main() {
  HostValidator validator({2, 4, 2, 32768});
  HostIngress request{Generation(0), address::ChannelId(1), 3, AxiId(7),
      PacketType::FlashIo, HostOperation::Read, 0, 64, false, 0, {}};
  assert(validator.validate(request));
  request.address_bytes = 1; assert(!validator.validate(request));
  request.address_bytes = 4096 - 64; request.size_bytes = 128;
  assert(!validator.validate(request));
  request.address_bytes = 0; request.size_bytes = 4096;
  assert(validator.validate(request));
  request.channel = address::ChannelId(2); assert(!validator.validate(request));
  request.channel = address::ChannelId(0); request.packet_type = PacketType::Reserved;
  assert(!validator.validate(request));

  OrderingScoreboard scoreboard;
  OrderingKey id0{address::ChannelId(0), 0, AxiId(3)};
  OrderingKey id1{address::ChannelId(0), 0, AxiId(4)};
  assert(scoreboard.reserve(Token(1), id0, 0, 64));
  assert(scoreboard.reserve(Token(2), id0, 64, 64));
  assert(scoreboard.reserve(Token(3), id1, 0, 64));
  assert(scoreboard.mark_ready(Token(2)) && scoreboard.release_ready().empty());
  assert(scoreboard.mark_ready(Token(3)) && scoreboard.release_ready().empty());
  OrderingKey other_axi{address::ChannelId(0), 1, AxiId(4)};
  assert(scoreboard.reserve(Token(4), other_axi, 0, 64));
  assert(scoreboard.mark_ready(Token(4)));
  const auto isolated = scoreboard.release_ready();
  assert(isolated.size() == 1 && isolated[0] == Token(4));
  assert(scoreboard.mark_ready(Token(1)));
  const auto released = scoreboard.release_ready();
  assert(released.size() == 3 && released[0] == Token(1) &&
         released[1] == Token(2) && released[2] == Token(3));

  assert(decode_packet_type(0) == PacketType::FlashIo);
  assert(!decode_packet_type(3));
  assert(decode_admin_opcode(0x08) == AdminOpcode::ZoneRemapping);
  assert(!decode_admin_opcode(0x06));
  assert(valid_sideband_command(SidebandCommand::ManagementPort));
  assert(!wire_codec_available());

  std::vector<address::ChannelId> reset_intents;
  RegisterPort registers(2, 4, 8192,
      [&](address::ChannelId c) { reset_intents.push_back(c); });
  assert(registers.allows_flash(address::ChannelId(0), 0, 0, 8192));
  assert(!registers.allows_scratchpad(address::ChannelId(0), 0, 0, 64));
  RegisterTxn txn{address::ChannelId(0), 0, 0x000C, true, 1};
  assert(registers.access(txn).status == RegisterStatus::Success && registers.enabled(txn.channel));
  txn.value = 2; assert(registers.access(txn).status == RegisterStatus::ReservedBits);
  registers.set_ready(txn.channel, true);
  txn = {address::ChannelId(0), 0, 0x0010, true, 0};
  assert(registers.access(txn).status == RegisterStatus::ReadOnly);
  txn = {address::ChannelId(0), 0, 0x0014, true, 0x484246};
  assert(registers.access(txn).status == RegisterStatus::Success && reset_intents.size() == 1);
  registers.set_critical_error(address::ChannelId(0));
  registers.reset_hbf(address::ChannelId(0));
  txn = {address::ChannelId(0), 0, 0x0018, false, 0};
  assert(registers.access(txn).value == 1);  // CES survives HBF reset.
  txn = {address::ChannelId(0), 1, 0x001C, true, 64};
  assert(registers.access(txn).status == RegisterStatus::Success);
  assert(registers.access({address::ChannelId(0), 1, 0x0024, true, 2}).status ==
         RegisterStatus::Success);
  assert(registers.allows_scratchpad(address::ChannelId(0), 1, 64, 128));
  txn.axi_interface = 2; txn.write = false;
  assert(registers.access(txn).status == RegisterStatus::Success &&
         registers.access(txn).value == 0);  // per-AXI ownership is isolated.
  txn = {address::ChannelId(0), 1, 0x0034, true, 64};
  assert(registers.access(txn).status == RegisterStatus::ReservedBits);
  txn.value = 4096;
  assert(registers.access(txn).status == RegisterStatus::Success);
  txn = {address::ChannelId(0), 0, 0x0000, true, 1};
  assert(registers.access(txn).status == RegisterStatus::ReadOnly);
}
