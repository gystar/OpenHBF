#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

#include "openhbx/hbf/address/channel_topology.h"
#include "openhbx/hbf/address/hbf_address_mapper.h"
#include "openhbx/hbf/host/host_protocol.h"

using namespace openhbx;
using namespace openhbx::hbf;

namespace {
class SyncController final : public host::IControllerPort {
 public:
  AdmissionResult submit(controller::ControllerRequest request, Cycle now) override {
    ++calls;
    const auto result = script.at(script_index++);
    if (result.code == AdmissionCode::Accepted && complete_synchronously) {
      sink({request.token, request.generation, controller::ControllerStatus::Success,
            true, PayloadHandle::from_bytes(std::vector<std::uint8_t>(64, 0x40 + calls)),
            now, controller::ControllerErrorInfo::None});
    }
    return result;
  }
  AdmissionResult submit_admin(Token, Generation, std::uint8_t, Cycle) override {
    return AdmissionResult::rejected(RejectionReason::InvalidArgument);
  }
  void reset(Generation, Cycle) override {}

  controller::ControllerCompletionSink sink;
  std::vector<AdmissionResult> script;
  std::size_t script_index{0};
  std::uint64_t calls{0};
  bool complete_synchronously{false};
};

void ready(host::HbfHostProtocol& protocol) {
  using host::LinkState;
  for (auto state : {LinkState::InitializationPhase1, LinkState::InitializationPhase2,
                     LinkState::InitializationPhase3, LinkState::InitializationPhase4,
                     LinkState::Ready})
    assert(protocol.set_channel_state(address::ChannelId(0), state, Generation(0)) ==
           host::LinkTransitionResult::Accepted);
  assert(protocol.registers().access({address::ChannelId(0), 0, 0x000C, true, 1}).status ==
         host::RegisterStatus::Success);
}

host::HostIngress read(std::uint64_t address, std::uint32_t bytes, std::uint16_t axi_id) {
  return {Generation(0), address::ChannelId(0), 0, host::AxiId(axi_id),
          host::PacketType::FlashIo, host::HostOperation::Read, address, bytes,
          false, 0, {}};
}
}

int main() {
  auto geometry = address::HbfGeometry::create({1, 1, 1, 1, 2, 1, 1, 2, 64, 1});
  assert(geometry);
  auto ownership = address::ChannelOwnershipProfile::synthetic_modulo(*geometry);
  assert(ownership);
  auto topology = address::ChannelTopology::create(*geometry, *ownership);
  assert(topology);
  address::HbfAddressMapper mapper(*geometry, *topology);
  SyncController controller;
  std::vector<host::HostResponse> responses;
  host::HbfHostProtocol protocol({1, 1, 4, geometry->local_capacity_bytes()}, mapper,
      controller, [&](host::HostResponse response) { responses.push_back(std::move(response)); });
  controller.sink = [&](controller::ControllerCompletion completion) {
    protocol.on_controller_completion(std::move(completion));
  };
  ready(protocol);

  // First-child Busy is externally Busy and must not allocate a token or
  // completion obligation. Retrying the same payload remains possible.
  controller.script = {AdmissionResult::busy(), AdmissionResult::accepted(),
                       AdmissionResult::accepted()};
  const auto before = protocol.snapshot();
  assert(protocol.submit_tracked(read(0, 128, 1), Cycle(1)).admission.code ==
         AdmissionCode::Busy);
  const auto busy = protocol.snapshot();
  assert(busy.accepted == before.accepted && busy.terminal == before.terminal &&
         busy.outstanding == before.outstanding && responses.empty());

  // Both children may complete synchronously from submit(). The root remains
  // one Accepted transaction and produces exactly one ordered response.
  controller.complete_synchronously = true;
  const auto admission = protocol.submit_tracked(read(0, 128, 1), Cycle(2));
  assert(admission.admission.code == AdmissionCode::Accepted &&
         admission.token.value() != 0);
  assert(responses.size() == 1 && responses.front().payload.size() == 128);
  const auto complete = protocol.snapshot();
  assert(complete.accepted == before.accepted + 1 &&
         complete.terminal == before.terminal + 1 && complete.outstanding == 0);

  // Busy after one accepted synchronous child is retained internally. It is
  // retried by pump without exposing a half-accepted transaction.
  controller.script = {AdmissionResult::accepted(), AdmissionResult::busy(),
                       AdmissionResult::accepted()};
  controller.script_index = 0;
  const auto response_count = responses.size();
  const auto staged = protocol.submit_tracked(read(256, 128, 2), Cycle(3));
  assert(staged.admission.code == AdmissionCode::Accepted);
  assert(responses.size() == response_count);
  protocol.pump(Cycle(4));
  assert(responses.size() == response_count + 1);
  const auto final = protocol.snapshot();
  assert(final.accepted == final.terminal + final.outstanding && final.outstanding == 0);
}
