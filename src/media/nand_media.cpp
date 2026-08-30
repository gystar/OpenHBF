#include "openhbf/media/nand_media.h"

#include <algorithm>
#include <utility>

namespace openhbf::media {
namespace {

Result<std::vector<ResourceId>> default_resources(const Geometry& geometry) {
  std::vector<ResourceId> resources;
  for (std::uint16_t core = 0; core < geometry.core_dies_per_channel; ++core) {
    for (std::uint16_t die = 0; die < geometry.dies_per_core; ++die) {
      for (std::uint16_t bank = 0; bank < geometry.banks_per_die; ++bank) {
        MediaAddress address{ChannelId(0), core, die, bank, 0, 0};
        address.channel = owner_channel(geometry, address);
        for (auto resource : {bank_array_resource(address),
                              channel_media_path_resource(address)}) {
          if (!resource) {
            return Result<std::vector<ResourceId>>::failure(resource.error());
          }
          resources.push_back(resource.value());
        }
      }
    }
  }
  std::sort(resources.begin(), resources.end());
  resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
  return Result<std::vector<ResourceId>>::success(std::move(resources));
}

}  // namespace

NandMedia::NandMedia(MediaConfig config, MediaTopology topology,
                     std::unique_ptr<IPageStore> pages,
                     NandTimingModel timing, EatTable eat,
                     ReliabilityModel reliability, BadBlockTable bbt,
                     RetirementMap retirement,
                     CommandDependencies dependencies)
    : config_(std::move(config)), topology_(std::move(topology)),
      pages_(std::move(pages)), state_(topology_), timing_(std::move(timing)),
      eat_(std::move(eat)), reliability_(std::move(reliability)),
      bbt_(std::move(bbt)), retirement_(std::move(retirement)),
      environment_(config_.thermal), stats_(config_.log_capacity),
      engine_(config_.max_in_flight, topology_, state_, *pages_, timing_, eat_,
              reliability_, bbt_, retirement_, environment_, stats_,
              std::move(dependencies)) {}

Result<std::unique_ptr<NandMedia>> NandMedia::create(
    MediaConfig config, CommandDependencies dependencies,
    std::vector<FaultRule> faults) {
  auto config_valid = validate(config);
  if (!config_valid) {
    return Result<std::unique_ptr<NandMedia>>::failure(config_valid.error());
  }
  auto topology = MediaTopology::create(config.geometry);
  if (!topology) {
    return Result<std::unique_ptr<NandMedia>>::failure(topology.error());
  }
  auto timing = NandTimingModel::make_synthetic_defaults(config.cell_mode);
  if (!timing) {
    return Result<std::unique_ptr<NandMedia>>::failure(timing.error());
  }
  auto resources = default_resources(config.geometry);
  if (!resources) {
    return Result<std::unique_ptr<NandMedia>>::failure(resources.error());
  }
  auto reliability = ReliabilityModel::make_defaults(
      config.cell_mode, config.reliability, std::move(faults));
  if (!reliability) {
    return Result<std::unique_ptr<NandMedia>>::failure(reliability.error());
  }
  auto bbt = BadBlockTable::create(config.geometry);
  if (!bbt) return Result<std::unique_ptr<NandMedia>>::failure(bbt.error());
  auto factory_bad = initialize_factory_bad_blocks(
      bbt.value(), config.geometry, config.reliability);
  if (!factory_bad) {
    return Result<std::unique_ptr<NandMedia>>::failure(factory_bad.error());
  }
  auto retirement = RetirementMap::create(config.geometry);
  if (!retirement) {
    return Result<std::unique_ptr<NandMedia>>::failure(retirement.error());
  }
  const PayloadMode payload_mode = config.payload_mode;
  try {
    return Result<std::unique_ptr<NandMedia>>::success(
        std::unique_ptr<NandMedia>(new NandMedia(
            std::move(config), std::move(topology.value()),
            make_page_store(payload_mode), std::move(timing.value()),
            EatTable(std::move(resources.value())),
            std::move(reliability.value()), std::move(bbt.value()),
            std::move(retirement.value()), std::move(dependencies))));
  } catch (const std::exception& error) {
    return Result<std::unique_ptr<NandMedia>>::failure(
        {ErrorCode::kInvalidArgument,
         std::string("failed to construct NAND Media: ") + error.what()});
  }
}

IssueResult NandMedia::try_issue(const MediaCommand& command, Cycle now) {
  return engine_.try_issue(command, now);
}

void NandMedia::on_event(const StageEvent& event, Cycle now) {
  engine_.on_event(event, now);
}

void NandMedia::cancel_generation(Generation generation, Cycle now) {
  engine_.cancel_generation(generation, now);
}

bool NandMedia::idle() const noexcept { return engine_.idle(); }

Result<BlockState> NandMedia::block_state(const MediaAddress& address) const {
  auto block = topology_.resolve_block(address);
  if (!block) return Result<BlockState>::failure(block.error());
  return Result<BlockState>::success(state_.block_state(block.value().key));
}

MediaSnapshot NandMedia::snapshot(Cycle now) const {
  return {topology_.snapshot(), state_.snapshot(), pages_->allocated_bytes(),
          engine_.in_flight(), bbt_.snapshot(), retirement_.retired_blocks(),
          environment_.snapshot(), stats_.snapshot(now)};
}

Result<std::vector<std::uint8_t>> NandMedia::save_metadata() const {
  return encode_metadata({config_.geometry, state_.export_blocks(),
                          bbt_.snapshot(), retirement_.snapshot()});
}

Result<void> NandMedia::restore_metadata(
    const std::vector<std::uint8_t>& bytes) {
  if (!idle()) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity, "cannot restore Media metadata with in-flight commands"});
  }
  const auto state_snapshot = state_.snapshot();
  if (state_snapshot.allocated_pages != 0 ||
      state_snapshot.allocated_blocks != 0 ||
      state_snapshot.reservations != 0 || pages_->allocated_bytes() != 0) {
    return Result<void>::failure(
        {ErrorCode::kIntegrity,
         "metadata restore requires a pristine Media; payload image restore is not implemented"});
  }
  auto decoded = decode_metadata(bytes, config_.geometry);
  if (!decoded) return Result<void>::failure(decoded.error());

  auto staged_bbt = BadBlockTable::create(config_.geometry);
  auto staged_retirement = RetirementMap::create(config_.geometry);
  if (!staged_bbt) return Result<void>::failure(staged_bbt.error());
  if (!staged_retirement)
    return Result<void>::failure(staged_retirement.error());
  auto bbt_valid = staged_bbt.value().replace(decoded.value().bbt);
  auto retirement_valid =
      staged_retirement.value().replace(decoded.value().retirement);
  if (!bbt_valid) return bbt_valid;
  if (!retirement_valid) return retirement_valid;

  // import_blocks validates into a temporary map before replacing its owner.
  // BBT and retirement have already been fully validated in temporary owners.
  auto blocks_valid = state_.import_blocks(decoded.value().blocks);
  if (!blocks_valid) return blocks_valid;
  bbt_ = std::move(staged_bbt.value());
  retirement_ = std::move(staged_retirement.value());
  return Result<void>::success();
}

}  // namespace openhbf::media
