#include <cstdlib>
#include <limits>
#include <vector>
#include "openhbx/media/nand/nand_flash_device.h"

using namespace openhbx;
using namespace openhbx::media;
using namespace openhbx::media::nand;

namespace {
void run(EventQueue& queue, Generation generation, std::uint64_t begin, std::uint64_t end) {
  for (std::uint64_t cycle = begin; cycle <= end; ++cycle)
    queue.dispatch_due(Cycle(cycle), EventPhase::MediaCommit, generation);
}
NandDeviceConfig make_config() {
  config::HbfGeometry g{1, 1, 1, 2, 2, 4, 4096, 16, 65536};
  return {g, PayloadMode::Sparse, StageTiming{}, {42, 0}, 8, false, 1, false};
}
}  // namespace

int main() {
  EventQueue queue;
  std::vector<MediaCompletion> done;
  NandFlashDevice media(make_config(), queue, HandlerId(70),
                        [&](MediaCompletion c) { done.push_back(std::move(c)); });
  const PhysicalAddress a{0, 0, 0, 0, 0};
  std::vector<std::uint8_t> bytes(4096, 0x5a);
  FlashCommand program{Token(1), Generation(0), CommandKind::Program, a,
                       PayloadHandle::from_bytes(bytes)};
  if (media.try_issue(program, Cycle(0)).code != AdmissionCode::Accepted) return EXIT_FAILURE;
  run(queue, Generation(0), 0, 24);
  if (media.page_is_valid(a) || !done.empty()) return EXIT_FAILURE;
  run(queue, Generation(0), 25, 30);
  if (!media.page_is_valid(a) || done.size() != 1 || done.back().status != MediaStatus::Success)
    return EXIT_FAILURE;

  FlashCommand read{Token(2), Generation(0), CommandKind::Read, a, {}};
  if (media.try_issue(read, Cycle(31)).code != AdmissionCode::Accepted) return EXIT_FAILURE;
  run(queue, Generation(0), 31, 50);
  if (done.size() != 2 || !done.back().data_valid || done.back().payload.bytes() != bytes)
    return EXIT_FAILURE;

  const auto block = *media.topology().flatten_block(a);
  media.reliability().force_failure(block, ras::ForcedFailure::Erase);
  FlashCommand erase_fail{Token(3), Generation(0), CommandKind::Erase, a, {}};
  if (media.try_issue(erase_fail, Cycle(51)).code != AdmissionCode::Accepted) return EXIT_FAILURE;
  run(queue, Generation(0), 51, 100);
  if (!media.page_is_valid(a) || done.back().status != MediaStatus::EraseFail ||
      media.block_erase_count(a) != 0) return EXIT_FAILURE;

  media.reliability().clear_forced_failure(block);
  FlashCommand reset_program{Token(4), Generation(0), CommandKind::Program,
                             {0, 0, 1, 0, 0}, PayloadHandle::from_bytes(bytes)};
  if (media.try_issue(reset_program, Cycle(101)).code != AdmissionCode::Accepted) return EXIT_FAILURE;
  media.reset(Generation(1), Cycle(102));
  run(queue, Generation(1), 102, 200);
  if (media.page_is_valid({0, 0, 1, 0, 0}) || done.back().status != MediaStatus::Aborted ||
      media.snapshot().inflight != 0) return EXIT_FAILURE;

  media.bad_block_table().mark(1, ras::BadBlockReason::Factory);
  FlashCommand bad{Token(5), Generation(1), CommandKind::Read, {0, 0, 0, 1, 0}, {}};
  if (media.try_issue(bad, Cycle(201)).code != AdmissionCode::Rejected) return EXIT_FAILURE;
  media.set_die_blocked(0, true);
  FlashCommand blocked{Token(6), Generation(1), CommandKind::Read, a, {}};
  const auto before_blocked = done.size();
  if (media.try_issue(blocked, Cycle(201)).code != AdmissionCode::Accepted ||
      done.size() != before_blocked) return EXIT_FAILURE;
  run(queue, Generation(1), 201, 210);
  if (done.size() != before_blocked + 1 ||
      done.back().status != MediaStatus::DieTemporarilyBlocked || done.back().data_valid)
    return EXIT_FAILURE;
  media.set_die_blocked(0, false);

  media.reliability().force_read(*media.topology().flatten_page(a), ras::RawReadClass::Retry);
  FlashCommand retry{Token(7), Generation(1), CommandKind::Read, a, {}};
  if (media.try_issue(retry, Cycle(211)).code != AdmissionCode::Accepted) return EXIT_FAILURE;
  run(queue, Generation(1), 211, 240);
  if (done.back().status != MediaStatus::RetrySuggested || done.back().data_valid ||
      !done.back().payload.empty()) return EXIT_FAILURE;

  const auto completions_before_overflow = done.size();
  const auto events_before_overflow = queue.snapshot().queued;
  FlashCommand overflow{Token(8), Generation(1), CommandKind::Read, a, {}};
  const auto overflow_result = media.try_issue(overflow,
      Cycle(std::numeric_limits<std::uint64_t>::max()));
  if (overflow_result.code != AdmissionCode::Rejected ||
      overflow_result.reason != RejectionReason::CapacityImpossible ||
      done.size() != completions_before_overflow ||
      queue.snapshot().queued != events_before_overflow || media.snapshot().inflight != 0)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
