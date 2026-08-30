#pragma once

#include <map>
#include <optional>
#include <vector>

#include "openhbx/common/admission.h"
#include "openhbx/hbf/address/hbf_address_mapper.h"
#include "openhbx/hbf/host/controller_port.h"
#include "openhbx/hbf/host/host_validator.h"
#include "openhbx/hbf/host/ordering_scoreboard.h"
#include "openhbx/hbf/host/packet_codec.h"
#include "openhbx/hbf/host/register_port.h"

namespace openhbx::hbf::host {

struct HostSnapshot {
  std::uint64_t accepted{0};
  std::uint64_t terminal{0};
  std::uint64_t busy{0};
  std::uint64_t rejected{0};
  std::uint64_t stale{0};
  std::uint64_t duplicate{0};
  std::size_t outstanding{0};
};

struct HostAdmission {
  AdmissionResult admission;
  Token token;
};

class HbfHostProtocol {
 public:
  HbfHostProtocol(HostProfile profile, const address::HbfAddressMapper& mapper,
                  IControllerPort& controller, HostResponseSink response,
                  ResetIntentSink reset_intent = {});
  AdmissionResult submit(const HostIngress& request, Cycle now);
  HostAdmission submit_tracked(const HostIngress& request, Cycle now);
  void on_controller_completion(controller::ControllerCompletion completion);
  void pump(Cycle now);
  LinkTransitionResult set_channel_state(address::ChannelId channel, LinkState state,
                                         Generation generation);
  void reset(Generation next_generation, Cycle now);
  RegisterPort& registers() noexcept { return registers_; }
  HostSnapshot snapshot() const noexcept;

 private:
  struct Record {
    HostIngress request;
    std::vector<controller::ControllerRequest> children;
    std::size_t next_issue{0};
    std::size_t accepted_children{0};
    std::size_t completed_children{0};
    std::vector<std::uint8_t> read_bytes;
    std::optional<controller::ControllerCompletion> failure;
    controller::ControllerCompletion completion;
    Cycle last_terminal_cycle;
    bool has_completion{false};
  };
  bool gate_open(const HostIngress& request) const noexcept;
  AdmissionResult issue_children(Token root, Cycle now, bool first_issue);
  void finish_if_ready(Token root);
  void drain_responses();
  static bool supported_admin_opcode(AdminOpcode opcode) noexcept;

  HostProfile profile_;
  const address::HbfAddressMapper& mapper_;
  IControllerPort& controller_;
  HostResponseSink response_;
  HostValidator validator_;
  OrderingScoreboard scoreboard_;
  RegisterPort registers_;
  Generation generation_{0};
  std::uint64_t next_token_{1};
  std::vector<LinkState> links_;
  std::vector<std::size_t> interface_outstanding_;
  std::map<std::uint64_t, Record> records_;
  struct ChildRef { Token root; std::size_t index{0}; };
  std::map<std::uint64_t, ChildRef> children_;
  bool dispatching_{false};
  bool resetting_{false};
  std::uint64_t accepted_{0}, terminal_{0}, busy_{0}, rejected_{0};
  std::uint64_t stale_{0}, duplicate_{0};
};

}  // namespace openhbx::hbf::host
