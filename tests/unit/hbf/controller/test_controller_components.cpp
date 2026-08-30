#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "openhbx/hbf/controller/bank_cache_manager.h"
#include "openhbx/hbf/controller/control_plane.h"
#include "openhbx/hbf/controller/dlu_accumulator.h"
#include "openhbx/hbf/controller/ecc_pipeline.h"
#include "openhbx/hbf/controller/flash_scheduler.h"

using namespace openhbx;
using namespace openhbx::hbf;
using namespace openhbx::hbf::address;
using namespace openhbx::hbf::controller;

namespace {
DluKey key(std::uint64_t page = 0) {
  return {{ChannelId(0), OwnedBankIndex(0), BlockIndex(0)}, PageIndex(page)};
}
PhysicalBank bank(std::uint64_t value = 0) {
  return {CoreDieIndex(0), DieIndex(0), BankIndex(value)};
}
PayloadHandle sector(std::uint8_t value) {
  return PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, value));
}
}

int main() {
  DluAccumulator accumulator(1, 10);
  for (std::uint64_t i = 0; i < 64; ++i) {
    auto r = accumulator.add_sector(key(), SectorIndex(i), sector(static_cast<std::uint8_t>(i)),
                                    Token(i + 1), Cycle(5));
    assert(r.code == (i == 63 ? AccumulateCode::Complete : AccumulateCode::Accepted));
    if (i == 63) assert(r.payload.size() == 4096 && r.payload.bytes()[64] == 1);
  }
  auto overlap = accumulator.add_sector(key(), SectorIndex(1), sector(9), Token(99), Cycle(6));
  assert(overlap.code == AccumulateCode::Overlap);
  auto forwarded = accumulator.probe_read(key(), SectorIndex(1));
  assert(forwarded.code == ProbeCode::Forward && forwarded.payload.bytes()[0] == 1);
  // A complete DLU has left the accumulation phase; backend backpressure must
  // not turn it into an accumulation timeout.
  assert(accumulator.expire(Cycle(100)).empty());
  accumulator.release(key());
  assert(accumulator.add_sector(key(), SectorIndex(2), sector(2), Token(2), Cycle(10)).code ==
         AccumulateCode::Accepted);
  assert(accumulator.expire(Cycle(19)).empty());
  auto expired = accumulator.expire(Cycle(20));
  assert(expired.size() == 1 && expired[0].tokens.size() == 1);

  bool rejected_cache = false;
  try { BankCacheManager invalid({bank()}, 1); } catch (const std::invalid_argument&) { rejected_cache = true; }
  assert(rejected_cache);
  BankCacheManager cache({bank()}, 2);
  auto c0 = cache.reserve(bank(), key()); auto c1 = cache.reserve(bank(), key(1));
  assert(c0 && c1 && !cache.reserve(bank(), key(2)));
  auto page = PayloadHandle::from_bytes(std::vector<std::uint8_t>(4096, 0x5a));
  assert(cache.fill(*c0, page));
  auto hit = cache.lookup(bank(), key());
  assert(hit && hit->payload.shares_storage_with(page));
  auto evicted = cache.reserve(bank(), key(2));
  assert(evicted && evicted->slot == c0->slot && evicted->epoch != c0->epoch);
  assert(!cache.fill(*c0, page));
  assert(cache.release(*evicted) && cache.free_count(bank()) == 1);

  FlashScheduler scheduler(4);
  assert(scheduler.enqueue({Token(1), bank(), WorkKind::Read, ReadMode::Batch}));
  assert(scheduler.enqueue({Token(2), bank(), WorkKind::Read, ReadMode::Regular}));
  auto selected = scheduler.select(); assert(selected && selected->token == Token(1));
  assert(!scheduler.select()); scheduler.complete(bank());
  assert(scheduler.select()->token == Token(2));
  scheduler.complete(bank());
  assert(scheduler.enqueue({Token(3), bank(), WorkKind::Program, ReadMode::Regular}));
  assert(scheduler.cancel(Token(3)) && scheduler.queued() == 0);

  EccPipeline ecc(1);
  auto corrected = ecc.decode({Token(3), pal::PalStatus::Corrected, true, page, Cycle(1)});
  assert(corrected.status == ControllerStatus::Corrected && corrected.data_valid);
  auto bad = ecc.decode({Token(3), pal::PalStatus::Uncorrectable, true, page, Cycle(1)});
  assert(!bad.data_valid && bad.payload.empty());
  assert(static_cast<std::uint8_t>(bad.status) == 0x4);
  auto refresh = ecc.decode({Token(4), pal::PalStatus::RefreshNotice, true, page, Cycle(1)});
  assert(static_cast<std::uint8_t>(refresh.status) == 0x5 && refresh.data_valid);
  auto retry = ecc.decode({Token(5), pal::PalStatus::RetrySuggested, false, {}, Cycle(1)});
  assert(static_cast<std::uint8_t>(retry.status) == 0x6 && !retry.data_valid &&
         retry.error_info == ControllerErrorInfo::RetryStageUnavailable);
  auto erased = ecc.decode({Token(6), pal::PalStatus::ReadErasedPage, false, {}, Cycle(1)});
  assert(static_cast<std::uint8_t>(erased.status) == 0x7 && !erased.data_valid);
  auto unusable = ecc.decode({Token(7), pal::PalStatus::CapacityUnusable, false, {}, Cycle(1)});
  assert(static_cast<std::uint8_t>(unusable.status) == 0x8 && !unusable.data_valid);
  auto blocked = ecc.decode({Token(8), pal::PalStatus::DieTemporarilyBlocked, false, {}, Cycle(1)});
  assert(static_cast<std::uint8_t>(blocked.status) == 0x9 && !blocked.data_valid);
  auto gap = ecc.decode({Token(9), pal::PalStatus::PathUnavailable, false, {}, Cycle(1)});
  assert(gap.status == ControllerStatus::UnsupportedSpecGap && !gap.data_valid);

  ControlPlane control(64);
  assert(control.write_scratchpad(1, {1, 2, 3}));
  assert(control.read_scratchpad(1, 3) == std::vector<std::uint8_t>({1, 2, 3}));
  assert(control.validate(AdminOpcode::SecureErase, false) == AdminResult::Unsupported);
  control.reset(true); assert(control.read_scratchpad(1, 3) == std::vector<std::uint8_t>({0, 0, 0}));
}
