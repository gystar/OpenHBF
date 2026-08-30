#include <cstdlib>
#include <iostream>
#include <utility>

#include "openhbx/hbf/address/block_sequence.h"
#include "openhbx/hbf/address/channel_topology.h"
#include "openhbx/hbf/address/hbf_address_mapper.h"

using namespace openhbx;
using namespace openhbx::hbf::address;

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #value "\n"; ++failures; \
} } while (false)
}

int main() {
  auto geometry = HbfGeometry::create({1, 1, 1, 3, 4, 1, 3, 5, 64, 3});
  CHECK(geometry);
  ChannelOwnershipProfile profile;
  profile.banks_by_channel = {{{CoreDieIndex(0), DieIndex(0), BankIndex(2)},
                               {CoreDieIndex(0), DieIndex(0), BankIndex(0)},
                               {CoreDieIndex(0), DieIndex(0), BankIndex(1)}}};
  auto topology = ChannelTopology::create(*geometry, std::move(profile));
  CHECK(topology);
  ZoneMap zones(geometry->blocks_per_bank());
  HbfAddressMapper mapper(*geometry, *topology, &zones);
  BlockSequence sequence(*geometry);
  const BlockKey key{ChannelId(0), OwnedBankIndex(1), BlockIndex(2)};

  for (std::uint64_t page = 0; page <= 3; ++page) {
    const auto reservation = sequence.reserve_program(key, PageIndex(page), Token(10 + page));
    CHECK(reservation);
    CHECK(sequence.complete_program(*reservation.reservation,
                                    page == 3 ? ProgramResult::Failure
                                              : ProgramResult::Success));
  }
  const auto replay = sequence.replay_addresses(key);
  CHECK(replay.size() == 4);
  const std::uint64_t l2 = 2 * 3 * 5 + 1;
  for (std::uint64_t page = 0; page < replay.size(); ++page) {
    CHECK(replay[page].value() == (l2 + page * 3) * 64 * 64);
  }
  CHECK(sequence.reserve_program(key, PageIndex(4), Token(20)).error ==
        SequenceError::ReplayRequired);
  for (std::uint64_t page = 0; page <= 3; ++page) {
    const auto reservation = sequence.reserve_program(key, PageIndex(page), Token(30 + page));
    CHECK(reservation && reservation.reservation->replay);
    CHECK(sequence.complete_program(*reservation.reservation, ProgramResult::Success));
  }
  CHECK(sequence.snapshot(key).expected_page == 4);

  const auto before = mapper.map(ChannelId(0), LocalByteAddress(0));
  CHECK(before && before.mapped->address.block == BlockIndex(0));
  CHECK(zones.remap(ZoneId(0), ZoneId(1)));
  const auto after = mapper.map(ChannelId(0), LocalByteAddress(0));
  CHECK(after && after.mapped->address.block == BlockIndex(1));
  CHECK(after.mapped->epoch.value() == 1);
  CHECK(zones.payload_copy_events() == 0);
  const auto round_trip = mapper.reverse(after.mapped->address);
  CHECK(round_trip && round_trip.address->value() == 0);
  CHECK(zones.remap(ZoneId(1), ZoneId(2), true).error == ZoneError::Busy);
  CHECK(zones.epoch().value() == 1);
  CHECK(zones.retire(ZoneId(0)));
  CHECK(!mapper.map(ChannelId(0), LocalByteAddress(0)));

  if (failures != 0) std::cerr << failures << " address component checks failed\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
