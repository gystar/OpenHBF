#include <cassert>
#include <cstdint>
#include <iostream>

#include "openhbf/ftl/sequential_mapper.h"
#include "openhbf/media/nand_media.h"

namespace {

using openhbf::ftl::ProgramStatus;
using openhbf::ftl::SequentialMapper;
using openhbf::media::Address;
using openhbf::media::NandMedia;
using openhbf::media::Operation;
using openhbf::media::Status;

struct Fixture {
  openhbf::media::Geometry media_geometry{2, 2, 2, 2, 4};
  openhbf::media::Timing timing{10, 20, 100, 1};
  NandMedia media{media_geometry, timing};
  openhbf::ftl::Geometry ftl_geometry{2, 2, 2, 4, 64, 4, 2};
  SequentialMapper mapper{ftl_geometry, media};
};

void test_geometry_and_channel_isolation() {
  Fixture f;
  const auto a = f.mapper.map_dlu(0, 5);
  const auto b = f.mapper.map_dlu(1, 5);
  assert(a.channel == 0 && b.channel == 1);
  assert(a.die == b.die && a.bank == b.bank && a.page == b.page);
  assert(f.mapper.program_dlu(0, 0, 0xA0, 0).status == ProgramStatus::kSuccess);
  assert(f.mapper.program_dlu(1, 0, 0xB0, 0).status == ProgramStatus::kSuccess);
  assert(f.media.read(f.mapper.map_dlu(0, 0), 1000).data_signature == 0xA0);
  assert(f.media.read(f.mapper.map_dlu(1, 0), 1000).data_signature == 0xB0);
}

void test_auto_erase_and_sequential_program() {
  Fixture f;
  const auto page0 = f.mapper.program_dlu(0, 0, 10, 0);
  assert(page0.status == ProgramStatus::kSuccess && page0.auto_erased);

  // Lane zero advances by R5=4 DLUs to the next page.
  const auto skipped = f.mapper.program_dlu(0, 8, 30, page0.completion_cycle);
  assert(skipped.status == ProgramStatus::kWriteOrderViolation);
  assert(skipped.user_info == 1);
  const auto page1 = f.mapper.program_dlu(0, 4, 20, page0.completion_cycle);
  assert(page1.status == ProgramStatus::kSuccess && !page1.auto_erased);
  assert(f.media.expected_page(f.mapper.map_dlu(0, 0)) == 2);
}

void test_program_failure_and_host_replay() {
  Fixture f;
  const auto p0 = f.mapper.program_dlu(0, 0, 10, 0);
  assert(p0.status == ProgramStatus::kSuccess);
  const Address p1 = f.mapper.map_dlu(0, 4);
  f.media.inject_fault(
      {Operation::kProgram, p1, Status::kProgramFail, 0, true});
  const auto failed = f.mapper.program_dlu(0, 4, 20, p0.completion_cycle);
  assert(failed.status == ProgramStatus::kProgramFailReplay);
  assert(f.media.replay_required(p1));

  const auto followup = f.mapper.program_dlu(0, 8, 30, failed.completion_cycle);
  assert(followup.status == ProgramStatus::kWriteOrderViolation);
  assert(followup.user_info == 2);

  // Host replay starts at page zero; page-zero write auto-erases and clears
  // replay.
  const auto replay0 = f.mapper.program_dlu(0, 0, 11, failed.completion_cycle);
  assert(replay0.status == ProgramStatus::kSuccess && replay0.auto_erased);
  const auto replay1 = f.mapper.program_dlu(0, 4, 21, replay0.completion_cycle);
  assert(replay1.status == ProgramStatus::kSuccess);
  assert(!f.media.replay_required(p1));
}

void test_replay_address_formula() {
  Fixture f;
  // A1=9*64+7 points into DLU 9. R1*R2*R3=16.
  const auto range = f.mapper.calculate_replay_range(9 * 64 + 7);
  assert(range.logical_unit == 9);
  assert(range.block_number == 0);
  assert(range.failed_page_number == 9);
  assert(range.bank_number == 1);
  assert(range.start_dlu == 1);
  assert((range.dlus == std::vector<std::uint64_t>{1, 5, 9, 13}));
}

void test_bank_timing_and_two_page_cache() {
  Fixture f;
  auto p0 = f.mapper.program_dlu(0, 0, 10, 0);
  auto p1 = f.mapper.program_dlu(0, 4, 20, p0.completion_cycle);
  auto p2 = f.mapper.program_dlu(0, 8, 30, p1.completion_cycle);
  assert(p2.status == ProgramStatus::kSuccess);

  auto r0 = f.media.read(f.mapper.map_dlu(0, 0), p2.completion_cycle);
  auto r0_hit = f.media.read(f.mapper.map_dlu(0, 0), r0.completion_cycle);
  assert(!r0.cache_hit && r0_hit.cache_hit);
  assert(r0_hit.completion_cycle == r0.completion_cycle + 1);

  f.media.read(f.mapper.map_dlu(0, 4), r0_hit.completion_cycle);
  f.media.read(f.mapper.map_dlu(0, 8), r0_hit.completion_cycle);
  auto evicted = f.media.read(f.mapper.map_dlu(0, 0), r0_hit.completion_cycle);
  assert(!evicted.cache_hit);

  // A different bank has an independent EAT and can complete earlier.
  auto other = f.mapper.program_dlu(0, 1, 99, 0);
  assert(other.status == ProgramStatus::kSuccess);
  assert(other.completion_cycle < evicted.completion_cycle);
}

void test_read_faults_and_temporary_block() {
  Fixture f;
  const auto write = f.mapper.program_dlu(0, 0, 0xCAFE, 0);
  const Address page = f.mapper.map_dlu(0, 0);

  f.media.inject_fault({Operation::kRead, page, Status::kCecc, 0, true});
  auto cecc = f.media.read(page, write.completion_cycle);
  assert(cecc.status == Status::kCecc && cecc.data_valid);
  assert(cecc.data_signature == 0xCAFE);

  f.media.inject_fault({Operation::kRead, page, Status::kUecc, 0, true});
  auto uecc = f.media.read(page, cecc.completion_cycle);
  assert(uecc.status == Status::kUecc && !uecc.data_valid);

  f.media.inject_fault({Operation::kRead, page, Status::kReadRetry, 3, true});
  auto retry = f.media.read(page, uecc.completion_cycle);
  assert(retry.status == Status::kReadRetry && retry.retry_stage == 3);

  f.media.set_die_temporary_blocked(0, 0, true);
  assert(f.media.read(page, retry.completion_cycle).status ==
         Status::kTemporaryBlocked);
  f.media.set_die_temporary_blocked(0, 0, false);
}

void test_reduced_capacity_bitmap() {
  Fixture f;
  Address retired{0, 0, 1, 1, 0};
  f.media.retire_block(retired);
  assert(f.media.is_block_retired(retired));
  assert(f.media.read(retired, 0).status == Status::kCapacityUnusable);

  const auto bitmap = f.media.reduced_capacity_bitmap(0);
  // die 0, bank 1, block 1 -> bit 3 with two blocks per bank.
  assert((bitmap[0] & (1U << 3)) != 0);
  assert((bitmap[0] & 1U) == 0); // LSB remains block zero.
  assert(f.media.reduced_capacity_bitmap(1)[0] == 0);
}

} // namespace

int main() {
  test_geometry_and_channel_isolation();
  test_auto_erase_and_sequential_program();
  test_program_failure_and_host_replay();
  test_replay_address_formula();
  test_bank_timing_and_two_page_cache();
  test_read_faults_and_temporary_block();
  test_reduced_capacity_bitmap();
  std::cout << "FTL/media focused tests passed\n";
  return 0;
}
