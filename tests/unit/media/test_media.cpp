#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "openhbf/media/nand_media.h"

namespace {

using namespace openhbf;
using namespace openhbf::media;

struct Scheduled {
  Cycle cycle;
  Generation generation;
  StageEvent event;
};

MediaAddress address(std::uint32_t page = 0, std::uint16_t bank = 0) {
  return {ChannelId(0), 0, 0, bank, 0, page};
}

SharedPage pattern(std::uint8_t seed) {
  auto data = std::make_shared<PageData>();
  for (std::size_t i = 0; i < data->size(); ++i) {
    (*data)[i] = static_cast<std::uint8_t>(seed + i);
  }
  return data;
}

void run_scheduled(NandMedia& media, std::vector<Scheduled>& scheduled) {
  for (std::size_t index = 0; index < scheduled.size(); ++index) {
    media.on_event(scheduled[index].event, scheduled[index].cycle);
  }
  scheduled.clear();
}

void test_topology_and_resources() {
  Geometry geometry{2, 1, 1, 4, 3, 4};
  auto topology = MediaTopology::create(geometry);
  assert(topology);
  assert(topology.value().page_count() == 48);
  const MediaAddress last{ChannelId(1), 0, 0, 3, 2, 3};
  const auto resolved = topology.value().resolve(last);
  assert(resolved);
  assert(topology.value().decode(resolved.value().key).value() == last);

  const auto bank0 = bank_array_resource(address(0, 0)).value();
  const auto bank1 = bank_array_resource(address(0, 1)).value();
  EatTable eat({bank0, bank1});
  const auto first = eat.reserve_atomic({bank0}, Cycle(0), Duration(10));
  const auto conflict = eat.reserve_atomic({bank0}, Cycle(1), Duration(2));
  const auto parallel = eat.reserve_atomic({bank1}, Cycle(1), Duration(2));
  assert(first.value().start == Cycle(0) && first.value().end == Cycle(10));
  assert(conflict.value().start == Cycle(10));
  assert(parallel.value().start == Cycle(1));

  const MediaAddress ch0_bank0{ChannelId(0), 0, 0, 0, 0, 0};
  const MediaAddress ch1_bank1{ChannelId(1), 0, 0, 1, 0, 0};
  assert(channel_media_path_resource(ch0_bank0).value() !=
         channel_media_path_resource(ch1_bank1).value());
  const MediaAddress ch0_bank2{ChannelId(0), 0, 0, 2, 0, 0};
  MediaAddress ch0_core1 = ch0_bank0;
  ch0_core1.core_die = 1;
  assert(channel_media_path_resource(ch0_bank0).value() ==
         channel_media_path_resource(ch0_bank2).value());
  assert(channel_media_path_resource(ch0_bank0).value() !=
         channel_media_path_resource(ch0_core1).value());
  assert(bank_array_resource(ch0_bank0).value() !=
         bank_array_resource(ch1_bank1).value());
  assert(!topology.value().resolve(
      {ChannelId(1), 0, 0, 0, 0, 0}));

  Geometry invalid_ncdu{2, 3, 1, 1, 1, 1};
  assert(!validate(invalid_ncdu));
}

void test_multi_core_die_channel_slices() {
  // Two physical Core Die domains, two Channels and four Banks per Die.
  // Channel is an owner view, so it must not multiply physical capacity.
  const Geometry geometry{2, 2, 2, 4, 3, 5};
  const auto topology = MediaTopology::create(geometry).value();
  assert(topology.block_count() == 48);
  assert(topology.page_count() == 240);

  for (std::uint16_t core = 0; core < geometry.core_dies_per_channel; ++core) {
    for (std::uint16_t die = 0; die < geometry.dies_per_core; ++die) {
      for (std::uint16_t bank = 0; bank < geometry.banks_per_die; ++bank) {
        const MediaAddress owned{ChannelId(bank % geometry.channels), core, die,
                                 bank, 2, 4};
        const auto resolved = topology.resolve(owned);
        assert(resolved);
        assert(topology.decode(resolved.value().key).value() == owned);

        MediaAddress forged = owned;
        forged.channel = ChannelId(1U - owned.channel.value());
        assert(!topology.resolve(forged));
      }
    }
  }

  auto bbt = BadBlockTable::create(geometry).value();
  const MediaAddress ch0_block{ChannelId(0), 1, 1, 2, 1, 0};
  assert(bbt.mark_bad(ch0_block, BadReason::Wear, Cycle(3), 9));
  MediaAddress forged_bbt = ch0_block;
  forged_bbt.channel = ChannelId(1);
  assert(!bbt.is_bad(forged_bbt));
  assert(bbt.snapshot().entries.size() == 1);

  auto retirement = RetirementMap::create(geometry).value();
  const MediaAddress ch1_core{ChannelId(1), 1, 0, 1, 0, 0};
  assert(retirement.retire({ScopeKind::CoreDie, ch1_core},
                           RetireReason::PathFailed, Cycle(4)));
  // One Channel owns 2 of 4 Banks in this Core Die domain:
  // 2 Dies x 2 Banks x 3 Blocks = 12 physical Blocks.
  assert(retirement.retired_blocks() == 12);
  assert(retirement.is_retired({ChannelId(1), 1, 1, 3, 2, 0}).value());
  assert(!retirement.is_retired({ChannelId(0), 1, 1, 2, 2, 0}).value());
  assert(retirement.bitmap_page(ChannelId(1), 0).value().bytes !=
         retirement.bitmap_page(ChannelId(0), 0).value().bytes);
}

void test_eat_parallelism_profile() {
  const MediaAddress ch0_core0_bank0{ChannelId(0), 0, 0, 0, 0, 0};
  const MediaAddress ch0_core0_bank2{ChannelId(0), 0, 0, 2, 0, 0};
  const MediaAddress ch1_core0_bank1{ChannelId(1), 0, 0, 1, 0, 0};
  const MediaAddress ch0_core1_bank0{ChannelId(0), 1, 0, 0, 0, 0};

  const auto bank0 = bank_array_resource(ch0_core0_bank0).value();
  const auto bank2 = bank_array_resource(ch0_core0_bank2).value();
  const auto ch0_core0 =
      channel_media_path_resource(ch0_core0_bank0).value();
  const auto ch1_core0 =
      channel_media_path_resource(ch1_core0_bank1).value();
  const auto ch0_core1 =
      channel_media_path_resource(ch0_core1_bank0).value();
  EatTable eat({bank0, bank2, ch0_core0, ch1_core0, ch0_core1});

  // Array stages serialize only at the same physical Bank.
  assert(eat.reserve_atomic({bank0}, Cycle(0), Duration(10)).value().start ==
         Cycle(0));
  assert(eat.reserve_atomic({bank0}, Cycle(1), Duration(2)).value().start ==
         Cycle(10));
  assert(eat.reserve_atomic({bank2}, Cycle(1), Duration(2)).value().start ==
         Cycle(1));

  // Data transfer serializes within one (Channel, Core Die) path.
  assert(eat.reserve_atomic({ch0_core0}, Cycle(0), Duration(10)).value().start ==
         Cycle(0));
  assert(eat.reserve_atomic({ch0_core0}, Cycle(1), Duration(2)).value().start ==
         Cycle(10));
  // A different Channel or NCDU Core Die domain has an independent path.
  assert(eat.reserve_atomic({ch1_core0}, Cycle(1), Duration(2)).value().start ==
         Cycle(1));
  assert(eat.reserve_atomic({ch0_core1}, Cycle(1), Duration(2)).value().start ==
         Cycle(1));

  const auto timing =
      NandTimingModel::make_synthetic_defaults(CellMode::Slc).value();
  const auto read = timing
                        .stages_for(MediaOp::Read, ch0_core0_bank0,
                                    PageClass::Default, 25.0)
                        .value();
  assert(read.stages[0].kind == StageKind::ReadSense);
  assert(read.stages[0].resources == std::vector<ResourceId>{bank0});
  assert(read.stages[1].kind == StageKind::ReadDataOut);
  assert(read.stages[1].resources == std::vector<ResourceId>{ch0_core0});
}

void test_tlc_default_layout() {
  auto timing = NandTimingModel::make_synthetic_defaults(CellMode::Tlc).value();
  const auto lsb = timing.stages_for(MediaOp::Program, address(0),
      PageClass::Default, 25.0).value();
  const auto csb = timing.stages_for(MediaOp::Program, address(1),
      PageClass::Default, 25.0).value();
  const auto msb = timing.stages_for(MediaOp::Program, address(2),
      PageClass::Default, 25.0).value();
  assert(lsb.stages[1].duration < csb.stages[1].duration);
  assert(csb.stages[1].duration < msb.stages[1].duration);
}

void test_bbt_retirement_and_environment() {
  Geometry geometry{1, 1, 1, 2, 4, 4};
  auto bbt = BadBlockTable::create(geometry).value();
  assert(bbt.mark_bad(address(), BadReason::Factory, Cycle(1), 17));
  assert(bbt.is_bad(address(3)).value());
  assert(!bbt.is_bad({ChannelId(2), 0, 0, 0, 0, 0}));
  assert(bbt.snapshot().version == 1);
  assert(bbt.snapshot().entries.front().pe_cycles == 17);
  assert(bbt.mark_bad(address(), BadReason::Wear, Cycle(2), 18).value() ==
         MarkResult::AlreadyBad);
  assert(bbt.snapshot().version == 1);

  auto retired = RetirementMap::create(geometry).value();
  assert(retired.retire({ScopeKind::Bank, address(0, 1)},
                        RetireReason::PathFailed, Cycle(9)));
  assert(retired.retired_blocks() == 4);
  assert(retired.is_retired(address(0, 1)).value());
  assert(!retired.is_retired(address(0, 0)).value());
  assert(retired.snapshot().entries.front().reason == RetireReason::PathFailed);

  ThermalConfig thermal{50.0, 70.0, 2, 1};
  DieEnvironment environment(thermal);
  assert(environment.update_temperature(55.0, Cycle(4)));
  assert(environment.stage_modifier(Duration(3)).value().duration == Duration(6));
  const DieKey die{0, 0};
  assert(environment.set_die_state(die, DieState::Recovering));
  assert(environment.can_accept(die, MediaOp::Read).status ==
         MediaStatus::DieRecovering);

  assert(!environment.update_temperature(70.0, Cycle(3)));
  assert(environment.update_temperature(70.0, Cycle(5)));
  assert(environment.snapshot().thermal_mode == ThermalMode::Cattrip);
  assert(environment.can_accept(die, MediaOp::Read).status ==
         MediaStatus::DieFailed);

  const auto path = channel_media_path_resource(address()).value();
  assert(!environment.observe_path_fault(path, PathFaultKind::Persistent,
                                         Cycle(6), Generation(1)));
  environment.reset_generation(Generation(1));
  assert(environment.observe_path_fault(path, PathFaultKind::Persistent,
                                        Cycle(6), Generation(1)));
  assert(environment.snapshot().path_faults.size() == 1);
}

void test_timing_reliability_and_stats() {
  assert(!NandTimingModel::make_synthetic_defaults(CellMode::Slc, 0));
  const auto slc = NandTimingModel::make_synthetic_defaults(CellMode::Slc, 3).value();
  const auto read = slc.stages_for(MediaOp::Read, address(), PageClass::Default,
                                   25.0).value();
  assert(read.stages.size() == 2);
  assert(read.stages.front().duration == Duration(8334));
  assert(!slc.stages_for(MediaOp::Read, address(), PageClass::Lsb, 25.0));
  assert(!slc.stages_for(MediaOp::Read, address(), PageClass::Default,
                         std::numeric_limits<double>::infinity()));

  ReliabilityConfig config;
  config.seed = 99;
  config.corrected_error_threshold = 4;
  config.retry_error_threshold = 2;
  config.refresh_read_threshold = 3;
  FaultRule rule;
  rule.kind = FaultKind::ReadErrors;
  rule.address = address();
  rule.stage = StageKind::ReadSense;
  rule.raw_errors = 5;
  auto reliability = ReliabilityModel::create(
      ReliabilityParams::make(100, 0.0, 0.0, 0.0), config, {rule}).value();
  ReliabilityInput input{MediaToken(1), address(), MediaOp::Read,
                         StageKind::ReadSense, 0, 0, 0, 0, 25.0};
  const auto first = reliability.evaluate_read(input);
  const auto replay = reliability.evaluate_read(input);
  assert(first.raw_errors == replay.raw_errors && first.raw_errors == 5);
  assert(first.retry_recommended && first.uncorrectable && first.explicit_fault);
  assert(reliability.on_successful_sense(address(), 7, 3).has_value());
  assert(!reliability.on_successful_sense(address(), 7, 4));

  MediaStats stats(2);
  const MediaCommand command{MediaOp::Read, address(), Generation(2),
                             RequestToken(4), {}, PageClass::Default};
  stats.on_issue(SubmitState::Accepted, MediaToken(3), command, Cycle(1),
                 MediaStatus::Success);
  stats.on_stage(MediaToken(3), Generation(2), address(), MediaOp::Read,
                 StageKind::ReadSense, Cycle(1), Cycle(4));
  MediaCompletion completion{MediaToken(3), RequestToken(4), Generation(2),
                             MediaOp::Read, address(), MediaStatus::Success,
                             Cycle(1), Cycle(4), {}, 2, false};
  stats.on_complete(completion);
  const auto snapshot = stats.snapshot(Cycle(5));
  assert(snapshot.accepted == 1 && snapshot.terminal == 1);
  assert(snapshot.read.successful_bytes == kPageBytes);
  assert(snapshot.stage_cycles == 3 && snapshot.raw_errors == 2);
  assert(snapshot.dropped_log_records == 1);
  const auto log = stats.drain_log();
  assert(log.size() == 2 && log[0].sequence == 0 && log[1].sequence == 1);
}

void test_async_program_read_erase() {
  std::vector<Scheduled> scheduled;
  std::vector<MediaCompletion> completions;
  std::uint64_t next_event = 1;
  CommandDependencies dependencies;
  dependencies.event_handler = EventHandlerId(5);
  dependencies.schedule = [&](Cycle cycle, EventPhase phase,
                              Generation generation, EventHandlerId,
                              EventPayload payload) {
    assert(phase == EventPhase::MediaCommit);
    scheduled.push_back({cycle, generation, payload.get<StageEvent>()});
    return EventToken(next_event++);
  };
  dependencies.complete = [&](MediaCompletion completion) {
    completions.push_back(std::move(completion));
  };

  MediaConfig config;
  config.geometry = {1, 1, 1, 2, 2, 4};
  config.cell_mode = CellMode::Slc;
  auto media = NandMedia::create(config, dependencies);
  assert(media);

  const Generation generation(1);
  const RequestToken origin(7);
  MediaCommand program{MediaOp::Program, address(0), generation, origin,
                       pattern(11), PageClass::Default};
  const auto issued = media.value()->try_issue(program, Cycle(0));
  assert(issued.state == SubmitState::Accepted);
  assert(completions.empty());
  assert(media.value()->snapshot(Cycle(1)).payload_bytes == 0);
  assert(scheduled.size() == 1);
  run_scheduled(*media.value(), scheduled);
  const Cycle program_done = completions.back().completed_cycle;
  assert(completions.size() == 1);
  assert(completions.back().status == MediaStatus::Success);
  assert(media.value()->snapshot(program_done).payload_bytes ==
         kPageBytes);

  MediaCommand read{MediaOp::Read, address(0), generation, RequestToken(8),
                    {}, PageClass::Default};
  assert(media.value()->try_issue(read, Cycle(1)).state == SubmitState::Accepted);
  assert(completions.size() == 1);
  assert(scheduled.size() == 1);
  run_scheduled(*media.value(), scheduled);
  assert(completions.size() == 2);
  assert(completions.back().payload && completions.back().payload->materialized);
  assert(*completions.back().payload->bytes == *program.payload);

  MediaCommand erase{MediaOp::Erase, address(0), generation, RequestToken(9),
                     {}, PageClass::Default};
  assert(media.value()->try_issue(erase, Cycle(2)).state == SubmitState::Accepted);
  assert(scheduled.size() == 1);
  run_scheduled(*media.value(), scheduled);
  const Cycle erase_done = completions.back().completed_cycle;
  assert(completions.back().status == MediaStatus::Success);
  assert(media.value()->snapshot(erase_done).payload_bytes == 0);

  MediaCommand bad_order{MediaOp::Program, address(2), generation,
                         RequestToken(10), pattern(2), PageClass::Default};
  assert(media.value()->try_issue(bad_order, Cycle(3)).status ==
         MediaStatus::ProgramOrderViolation);
}

void test_cancel_is_exactly_once() {
  std::vector<Scheduled> scheduled;
  std::vector<MediaCompletion> completions;
  CommandDependencies dependencies;
  dependencies.event_handler = EventHandlerId(6);
  dependencies.schedule = [&](Cycle cycle, EventPhase, Generation generation,
                              EventHandlerId, EventPayload payload) {
    scheduled.push_back({cycle, generation, payload.get<StageEvent>()});
    return EventToken(scheduled.size());
  };
  dependencies.complete = [&](MediaCompletion completion) {
    completions.push_back(std::move(completion));
  };
  MediaConfig config;
  config.geometry = {1, 1, 1, 1, 1, 2};
  auto media = NandMedia::create(config, dependencies).value();
  const Generation generation(3);
  const auto issued = media->try_issue(
      {MediaOp::Program, address(), generation, RequestToken(20), pattern(1),
       PageClass::Default},
      Cycle(0));
  assert(issued.state == SubmitState::Accepted);
  media->cancel_generation(generation, Cycle(1));
  assert(completions.size() == 1);
  assert(completions.front().status == MediaStatus::Aborted);
  assert(media->idle());
  assert(media->snapshot(Cycle(1)).payload_bytes == 0);
  const auto replacement = media->try_issue(
      {MediaOp::Program, address(), Generation(4), RequestToken(21), pattern(2),
       PageClass::Default}, Cycle(1));
  assert(replacement.state == SubmitState::Accepted);
  assert(scheduled.back().cycle == Cycle(4001));
  run_scheduled(*media, scheduled);
  assert(completions.size() == 2);
  assert(completions.back().status == MediaStatus::Success);
}

void test_fault_and_environment_are_terminal() {
  std::vector<Scheduled> scheduled;
  std::vector<MediaCompletion> completions;
  CommandDependencies dependencies;
  dependencies.event_handler = EventHandlerId(8);
  dependencies.schedule = [&](Cycle cycle, EventPhase, Generation generation,
                              EventHandlerId, EventPayload payload) {
    scheduled.push_back({cycle, generation, payload.get<StageEvent>()});
    return EventToken(scheduled.size());
  };
  dependencies.complete = [&](MediaCompletion completion) {
    completions.push_back(std::move(completion));
  };
  MediaConfig config;
  config.geometry = {1, 1, 1, 1, 1, 2};
  FaultRule program_fault;
  program_fault.kind = FaultKind::ProgramFailure;
  program_fault.address = address();
  program_fault.stage = StageKind::ProgramArray;
  auto media = NandMedia::create(config, dependencies, {program_fault}).value();
  assert(media->try_issue({MediaOp::Program, address(), Generation(1),
                           RequestToken(40), pattern(3), PageClass::Default},
                          Cycle(0)).state == SubmitState::Accepted);
  assert(scheduled.size() == 1);
  media->on_event(scheduled[0].event, scheduled[0].cycle);
  assert(completions.empty() && scheduled.size() == 2);
  media->on_event(scheduled[1].event, scheduled[1].cycle);
  assert(completions.size() == 1);
  assert(completions[0].status == MediaStatus::ProgramFailure);
  assert(scheduled.size() == 2);
  assert(media->snapshot(scheduled[1].cycle).payload_bytes == 0);

  scheduled.clear();
  completions.clear();
  auto healthy = NandMedia::create(config, dependencies).value();
  assert(healthy->try_issue({MediaOp::Program, address(), Generation(2),
                             RequestToken(41), pattern(4), PageClass::Default},
                            Cycle(0)).state == SubmitState::Accepted);
  assert(healthy->environment().update_temperature(106.0, Cycle(1)));
  healthy->on_event(scheduled.front().event, scheduled.front().cycle);
  assert(completions.size() == 1);
  assert(completions.front().status == MediaStatus::DieFailed);
  assert(healthy->snapshot(scheduled.front().cycle).payload_bytes == 0);
}

void test_factory_bad_and_metadata_persistence() {
  std::vector<Scheduled> scheduled;
  std::vector<MediaCompletion> completions;
  CommandDependencies dependencies;
  dependencies.event_handler = EventHandlerId(7);
  dependencies.schedule = [&](Cycle cycle, EventPhase, Generation generation,
                              EventHandlerId, EventPayload payload) {
    scheduled.push_back({cycle, generation, payload.get<StageEvent>()});
    return EventToken(scheduled.size());
  };
  dependencies.complete = [&](MediaCompletion completion) {
    completions.push_back(std::move(completion));
  };
  MediaConfig config;
  config.geometry = {1, 1, 1, 1, 4, 2};
  config.reliability.factory_bad_blocks = {address(0)};
  auto media = NandMedia::create(config, dependencies).value();
  assert(media->bad_blocks().is_bad(address()).value());

  // Use a non-factory-bad block, erase it and verify every normal block owns
  // PEC before any BBT entry exists for it.
  MediaAddress normal = address();
  normal.block = 1;
  const auto erase = media->try_issue(
      {MediaOp::Erase, normal, Generation(1), RequestToken(30), {},
       PageClass::Default},
      Cycle(0));
  assert(erase.state == SubmitState::Accepted);
  run_scheduled(*media, scheduled);
  const Cycle erase_done = completions.back().completed_cycle;
  assert(media->snapshot(erase_done).state.allocated_blocks == 1);

  assert(media->retirement().retire(
      {ScopeKind::Block, normal}, RetireReason::Administrative, Cycle(12)));
  const auto image = media->save_metadata();
  assert(image);

  auto restored = NandMedia::create(config, dependencies).value();
  assert(restored->restore_metadata(image.value()));
  assert(restored->bad_blocks().is_bad(address()).value());
  assert(restored->retirement().is_retired(normal).value());
  assert(restored->retirement().snapshot().entries.front().reason ==
         RetireReason::Administrative);
  const auto roundtrip = decode_metadata(image.value(), config.geometry);
  assert(roundtrip && roundtrip.value().blocks.size() == 1);
  assert(roundtrip.value().blocks.front().state.program_erase_count == 1);

  auto corrupted = image.value();
  corrupted.back() ^= 0x80U;
  const auto before = restored->retirement().snapshot().version;
  assert(!restored->restore_metadata(corrupted));
  assert(restored->retirement().snapshot().version == before);
}

}  // namespace

int main() {
  test_topology_and_resources();
  test_multi_core_die_channel_slices();
  test_eat_parallelism_profile();
  test_tlc_default_layout();
  test_bbt_retirement_and_environment();
  test_timing_reliability_and_stats();
  test_async_program_read_erase();
  test_cancel_is_exactly_once();
  test_fault_and_environment_are_terminal();
  test_factory_bad_and_metadata_persistence();
}
