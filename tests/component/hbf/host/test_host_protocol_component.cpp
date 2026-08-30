#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

#include "openhbx/hbf/address/hbf_address_mapper.h"
#include "openhbx/hbf/address/block_sequence.h"
#include "openhbx/hbf/controller/base_die_flash_controller.h"
#include "openhbx/hbf/host/host_protocol.h"
#include "openhbx/interconnect/fabric.h"
#include "openhbx/interconnect/tsv_repair.h"
#include "openhbx/pal/flash_pal.h"
#include "openhbx/pal/media_port.h"
#include "openhbx/system/event_queue.h"

using namespace openhbx;
using namespace openhbx::hbf;
using namespace openhbx::hbf::address;
using namespace openhbx::hbf::controller;
using namespace openhbx::hbf::host;

namespace {
class ControlledController final : public IControllerPort {
 public:
  AdmissionResult submit(ControllerRequest request, Cycle) override {
    auto result = next;
    if (!scripted.empty()) { result = scripted.front(); scripted.erase(scripted.begin()); }
    if (result.code == AdmissionCode::Accepted) submitted.push_back(std::move(request));
    return result;
  }
  AdmissionResult submit_admin(Token token, Generation generation, std::uint8_t,
                               Cycle) override {
    ControllerRequest request; request.token = token; request.generation = generation;
    if (next.code == AdmissionCode::Accepted) submitted.push_back(std::move(request));
    return next;
  }
  void reset(Generation generation, Cycle now) override {
    reset_generation = generation;
    for (auto token : reset_tokens)
      if (reset_sink) reset_sink({token, Generation(generation.value() - 1),
          ControllerStatus::Aborted, false, {}, now, ControllerErrorInfo::None});
    reset_tokens.clear();
  }
  AdmissionResult next{AdmissionResult::accepted()};
  std::vector<AdmissionResult> scripted;
  std::vector<ControllerRequest> submitted;
  std::vector<Token> reset_tokens;
  ControllerCompletionSink reset_sink;
  Generation reset_generation{0};
};

class NoopMedia final : public pal::IFlashMediaPort {
 public:
  AdmissionResult try_issue_media(media::FlashCommand, Cycle) override {
    return AdmissionResult::accepted();
  }
};

interconnect::FabricProfile fabric_profile() {
  interconnect::FabricProfile profile;
  profile.source = "open-hbx:synthetic-test"; profile.queue_depth = 2;
  profile.active_lanes = 8; profile.spare_lanes = 0;
  profile.bits_per_lane_per_cycle = 8; profile.efficiency_ppm = 1000000;
  profile.arbitration_cycles = 1; profile.propagation_cycles = 1;
  profile.routes = {{1, 0, {1}, {0}}}; return profile;
}

void make_ready(HbfHostProtocol& host, ChannelId channel,
                Generation generation = Generation(0)) {
  assert(host.set_channel_state(channel, LinkState::InitializationPhase1, generation) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(channel, LinkState::InitializationPhase2, generation) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(channel, LinkState::InitializationPhase3, generation) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(channel, LinkState::InitializationPhase4, generation) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(channel, LinkState::Ready, generation) ==
         LinkTransitionResult::Accepted);
}
}

int main() {
  auto geometry = HbfGeometry::create({2, 1, 1, 2, 2, 1, 1, 2, 64, 1});
  assert(geometry);
  auto ownership = ChannelOwnershipProfile::synthetic_modulo(*geometry);
  assert(ownership);
  auto topology = ChannelTopology::create(*geometry, *ownership);
  assert(topology);
  HbfAddressMapper mapper(*geometry, *topology);
  ControlledController controller;
  std::vector<HostResponse> responses;
  HbfHostProtocol host({2, 2, 2, geometry->local_capacity_bytes()}, mapper,
      controller, [&](HostResponse r) { responses.push_back(std::move(r)); });
  controller.reset_sink = [&](ControllerCompletion completion) {
    host.on_controller_completion(std::move(completion));
  };

  assert(host.set_channel_state(ChannelId(0), LinkState::Ready, Generation(0)) ==
         LinkTransitionResult::InvalidTransition);
  host.set_channel_state(ChannelId(0), LinkState::InitializationPhase1, Generation(0));
  HostIngress first{Generation(0), ChannelId(0), 0, AxiId(1), PacketType::FlashIo,
      host::HostOperation::Read, 0, 64, false, 0, {}};
  assert(host.submit(first, Cycle(0)).code == AdmissionCode::Rejected);
  assert(host.set_channel_state(ChannelId(0), LinkState::InitializationPhase3, Generation(0)) ==
         LinkTransitionResult::InvalidTransition);
  assert(host.set_channel_state(ChannelId(0), LinkState::InitializationPhase2, Generation(0)) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(ChannelId(0), LinkState::InitializationPhase3, Generation(0)) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(ChannelId(0), LinkState::InitializationPhase4, Generation(0)) ==
         LinkTransitionResult::Accepted);
  assert(host.set_channel_state(ChannelId(0), LinkState::Ready, Generation(0)) ==
         LinkTransitionResult::Accepted);
  assert(host.submit(first, Cycle(0)).code == AdmissionCode::Rejected);  // BUCC.EN=0
  auto enabled = host.registers().access({ChannelId(0), 0, 0x000C, true, 1});
  assert(enabled.status == RegisterStatus::Success);

  assert(host.submit(first, Cycle(1)).code == AdmissionCode::Accepted);
  auto second = first; second.axi_id = AxiId(2);
  assert(host.submit(second, Cycle(2)).code == AdmissionCode::Accepted);
  assert(controller.submitted.size() == 2);
  const auto t1 = controller.submitted[0].token;
  const auto t2 = controller.submitted[1].token;
  auto payload2 = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 2));
  host.on_controller_completion({t2, Generation(99), ControllerStatus::Corrected, true, payload2,
                                 Cycle(9), ControllerErrorInfo::None});
  assert(host.snapshot().stale == 1 && responses.empty());
  host.on_controller_completion({t2, Generation(0), ControllerStatus::Corrected, true, payload2,
                                 Cycle(10), ControllerErrorInfo::None});
  assert(responses.empty());  // different ID, but same 64 B address hazard.
  auto payload1 = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 1));
  host.on_controller_completion({t1, Generation(0), ControllerStatus::Success, true, payload1,
                                 Cycle(11), ControllerErrorInfo::None});
  assert(responses.size() == 2 && responses[0].token == t1 && responses[1].token == t2);
  assert(responses[1].command_status == 0x5 && responses[1].data_valid);

  // Equal AXI-local addresses occupy disjoint per-AXI and per-Channel slices.
  auto axi1 = first; axi1.axi_interface = 1; axi1.axi_id = AxiId(10);
  assert(host.submit(axi1, Cycle(11)).code == AdmissionCode::Accepted);
  make_ready(host, ChannelId(1));
  assert(host.registers().access({ChannelId(1), 0, 0x000C, true, 1}).status ==
         RegisterStatus::Success);
  auto channel1 = first; channel1.channel = ChannelId(1); channel1.axi_id = AxiId(11);
  assert(host.submit(channel1, Cycle(11)).code == AdmissionCode::Accepted);
  const auto& axi1_request = controller.submitted[2];
  const auto& channel1_request = controller.submitted[3];
  assert(!(axi1_request.dlu == controller.submitted[0].dlu));
  assert(channel1_request.dlu.block.channel == ChannelId(1));
  host.on_controller_completion({axi1_request.token, Generation(0), ControllerStatus::Success,
      true, payload1, Cycle(11), ControllerErrorInfo::None});
  host.on_controller_completion({channel1_request.token, Generation(0), ControllerStatus::Success,
      true, payload1, Cycle(11), ControllerErrorInfo::None});

  // FIOSA/FIOSZ are live per-AXI admission windows.
  assert(host.registers().access({ChannelId(0), 0, 0x0034, true, 4096}).status ==
         RegisterStatus::Success);
  assert(host.registers().access({ChannelId(0), 0, 0x003C, true, 1}).status ==
         RegisterStatus::Success);
  auto outside_fio = first;
  assert(host.submit(outside_fio, Cycle(11)).code == AdmissionCode::Rejected);
  auto inside_fio = first; inside_fio.address_bytes = 4096; inside_fio.axi_id = AxiId(12);
  assert(host.submit(inside_fio, Cycle(11)).code == AdmissionCode::Accepted);
  const auto inside_token = controller.submitted.back().token;
  host.on_controller_completion({inside_token, Generation(0), ControllerStatus::Success,
      true, payload1, Cycle(11), ControllerErrorInfo::None});
  assert(host.registers().access({ChannelId(0), 0, 0x0034, true, 0}).status ==
         RegisterStatus::Success);
  assert(host.registers().access({ChannelId(0), 0, 0x003C, true, 2}).status ==
         RegisterStatus::Success);
  const auto responses_before_write = responses.size();

  controller.next = AdmissionResult::busy();
  const auto before = host.snapshot();
  auto busy = first; busy.address_bytes = 64;
  auto busy_payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 9));
  busy.operation = host::HostOperation::Write; busy.payload = busy_payload;
  assert(host.submit(busy, Cycle(12)).code == AdmissionCode::Busy);
  const auto after = host.snapshot();
  assert(after.outstanding == before.outstanding &&
         busy.payload.shares_storage_with(busy_payload));
  controller.next = AdmissionResult::rejected(RejectionReason::InvalidArgument);
  assert(host.submit(busy, Cycle(12)).code == AdmissionCode::Rejected &&
         busy.payload.shares_storage_with(busy_payload));

  controller.next = AdmissionResult::accepted();
  auto write = first; write.operation = host::HostOperation::Write; write.address_bytes = 64;
  write.payload = PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 7));
  assert(host.submit(write, Cycle(13)).code == AdmissionCode::Accepted);
  const auto write_token = controller.submitted.back().token;
  assert(responses.size() == responses_before_write);  // non-posted write waits for S6 terminal.
  host.on_controller_completion({write_token, Generation(0), ControllerStatus::ProgramFail, false, {},
                                 Cycle(20), ControllerErrorInfo::None});
  assert(responses.size() == responses_before_write + 1 && responses.back().command_status == 0x7 &&
         !responses.back().data_valid);

  // A legal 128 B read is split into two S6 sector requests and produces one
  // ordered Host response with the sectors concatenated in address order.
  auto burst = first; burst.address_bytes = 256; burst.size_bytes = 128;
  const auto children_before = controller.submitted.size();
  assert(host.submit(burst, Cycle(20)).code == AdmissionCode::Accepted);
  assert(controller.submitted.size() == children_before + 2);
  const auto burst0 = controller.submitted[children_before].token;
  const auto burst1 = controller.submitted[children_before + 1].token;
  host.on_controller_completion({burst1, Generation(0), ControllerStatus::Success, true,
      PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x22)), Cycle(21),
      ControllerErrorInfo::None});
  host.on_controller_completion({burst0, Generation(0), ControllerStatus::Success, true,
      PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x11)), Cycle(22),
      ControllerErrorInfo::None});
  assert(responses.back().payload.size() == 128 && responses.back().payload.bytes()[0] == 0x11 &&
         responses.back().payload.bytes()[64] == 0x22);

  // Once the first child is accepted, later Busy is retained internally and
  // retried; the external transaction remains Accepted and never half-rolls back.
  auto retry_burst = first; retry_burst.address_bytes = 512; retry_burst.size_bytes = 128;
  controller.scripted = {AdmissionResult::accepted(), AdmissionResult::busy()};
  const auto retry_before = controller.submitted.size();
  assert(host.submit(retry_burst, Cycle(22)).code == AdmissionCode::Accepted);
  assert(controller.submitted.size() == retry_before + 1);
  const auto retry0 = controller.submitted.back().token;
  host.on_controller_completion({retry0, Generation(0), ControllerStatus::Success, true,
      PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x33)), Cycle(23),
      ControllerErrorInfo::None});
  assert(controller.submitted.size() == retry_before + 2);
  const auto retry1 = controller.submitted.back().token;
  host.on_controller_completion({retry1, Generation(0), ControllerStatus::Success, true,
      PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x44)), Cycle(24),
      ControllerErrorInfo::None});
  assert(responses.back().payload.size() == 128 && responses.back().payload.bytes()[64] == 0x44);

  // A rejection after the first accepted child becomes one terminal failure;
  // its timestamp waits for the accepted child terminal.
  auto rejected_burst = first; rejected_burst.address_bytes = 768;
  rejected_burst.size_bytes = 128;
  controller.scripted = {AdmissionResult::accepted(),
                         AdmissionResult::rejected(RejectionReason::InvalidArgument)};
  const auto rejected_before = controller.submitted.size();
  assert(host.submit(rejected_burst, Cycle(30)).code == AdmissionCode::Accepted);
  const auto rejected_child = controller.submitted[rejected_before].token;
  const auto response_before_reject_terminal = responses.size();
  host.on_controller_completion({rejected_child, Generation(0), ControllerStatus::Success, true,
      PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x55)), Cycle(40),
      ControllerErrorInfo::None});
  assert(responses.size() == response_before_reject_terminal + 1 &&
         responses.back().command_status == 0xF && responses.back().completed_at == Cycle(40));

  auto pending = first; pending.address_bytes = 1024; pending.size_bytes = 128;
  controller.scripted = {AdmissionResult::accepted(), AdmissionResult::busy()};
  assert(host.submit(pending, Cycle(21)).code == AdmissionCode::Accepted);
  const auto submitted_before_reset = controller.submitted.size();
  controller.reset_tokens = {controller.submitted.back().token};
  const auto responses_before_reset = responses.size();
  host.reset(Generation(1), Cycle(22));
  assert(controller.reset_generation == Generation(1));
  assert(controller.submitted.size() == submitted_before_reset &&
         responses.size() == responses_before_reset + 1);
  const auto snapshot = host.snapshot();
  assert(snapshot.accepted == snapshot.terminal && snapshot.outstanding == 0);
  host.on_controller_completion({controller.submitted.back().token, Generation(0),
      ControllerStatus::Success, true, payload1, Cycle(23), ControllerErrorInfo::None});
  assert(host.snapshot().stale == snapshot.stale + 1);

  // Real S6 public port: an Admin command completes synchronously as the
  // frozen Unsupported result and is routed exactly once by the Host facade.
  BlockSequence sequences(*geometry);
  EventQueue events;
  interconnect::TsvRepairManager repair(8, 0);
  interconnect::InterconnectFabric fabric(fabric_profile(), repair);
  NoopMedia media;
  HbfHostProtocol* real_host_ptr = nullptr;
  pal::FlashPal pal({1, 2, 2, 1, 512, 4096}, events, HandlerId(91), fabric, media,
                    [](pal::PalCompletion) {});
  const auto physical_bank = *topology->physical_bank(ChannelId(0), OwnedBankIndex(0));
  BaseDieFlashController real_controller({2, 20, 4, 2, 1, 64, 200},
      {physical_bank}, sequences, pal,
      [&](ControllerCompletion completion) {
        if (real_host_ptr) real_host_ptr->on_controller_completion(std::move(completion));
      });
  BaseDieControllerPort real_port(real_controller);
  std::vector<HostResponse> real_responses;
  HbfHostProtocol real_host({2, 1, 2, geometry->local_capacity_bytes()}, mapper,
      real_port, [&](HostResponse response) { real_responses.push_back(std::move(response)); });
  real_host_ptr = &real_host;
  make_ready(real_host, ChannelId(0));
  assert(real_host.registers().access({ChannelId(0), 0, 0x000C, true, 1}).status ==
         RegisterStatus::Success);
  HostIngress admin{Generation(0), ChannelId(0), 0, AxiId(9), PacketType::CsrAdmin,
      host::HostOperation::Admin, 0, 64, false, 0x03, {}};
  assert(real_host.submit(admin, Cycle(30)).code == AdmissionCode::Accepted);
  assert(real_responses.size() == 1 && real_responses[0].command_status == 0xD &&
         !real_responses[0].data_valid);
  for (const auto opcode : {std::uint8_t{0x01}, std::uint8_t{0x02}, std::uint8_t{0x04},
                            std::uint8_t{0x09}, std::uint8_t{0x20}}) {
    admin.admin_opcode = opcode; admin.axi_id = AxiId(10 + opcode);
    assert(real_host.submit(admin, Cycle(31)).code == AdmissionCode::Accepted);
    assert(real_responses.back().command_status == 0xD &&
           !real_responses.back().data_valid);
  }
  const auto real_snapshot = real_host.snapshot();
  assert(real_snapshot.accepted == 6 && real_snapshot.terminal == 6 &&
         real_snapshot.outstanding == 0);
}
