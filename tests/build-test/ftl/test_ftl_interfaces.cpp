#include <cassert>
#include <memory>

#include "openhbf/ftl/block_sequence.h"
#include "openhbf/ftl/geometry_mapper.h"

using namespace openhbf;
using namespace openhbf::ftl;

int main() {
  GeometryProfile profile;
  profile.r1 = 2;
  profile.r2 = 2;
  profile.r3 = 4;
  profile.r4 = 64;
  profile.r5 = 4;
  profile.core_die_count = 2;
  profile.dies_per_core = 2;
  profile.banks_per_die = 4;
  profile.blocks_per_bank = 8;
  profile.pages_per_block = 16;
  profile.channels = 2;

  GeometryMapper mapper(profile);
  const auto mapped = mapper.map(ChannelId(0), Dlu(9));
  assert(mapped);
  assert(mapped.value().l1 == 9);
  assert(mapped.value().b1 == 0);
  assert(mapped.value().p1 == 9);
  assert(mapped.value().bank_number == 1);
  assert(mapped.value().l2 == 1);
  assert(mapped.value().address.channel == ChannelId(0));

  auto payload = std::make_shared<Payload4KiB>();
  ProgramDluRequest request{RequestToken(1), ChannelId(0), Dlu(0), payload};
  assert(request.payload->size() == 4096);

  BlockSequenceTable sequences;
  const BlockKey block{ChannelId(0), 0, 0, 0, 0};
  const auto reservation = sequences.reserve(block, 0, FtlToken(1));
  assert(reservation);
  assert(!sequences.reserve(block, 1, FtlToken(2)));
  assert(sequences.complete(FtlToken(1), false, 0));
  assert(sequences.inspect(block)->mode == SequenceMode::ReplayRequired);
  assert(!sequences.inspect(block)->reservation);
}
