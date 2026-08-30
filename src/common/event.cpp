#include "openhbf/common/event.h"

namespace openhbf {

Event::Event(EventEnvelope envelope, EventPayload payload)
    : envelope_(std::move(envelope)), payload_(std::move(payload)) {
  if (envelope_.sequence == 0 || envelope_.token.value() == 0 ||
      envelope_.handler.value() == 0) {
    throw std::invalid_argument(
        "event sequence, token, and handler must be non-zero");
  }
  if (event_phase_order(envelope_.phase) >
      event_phase_order(EventPhase::FinalDelivery)) {
    throw std::invalid_argument("invalid event phase");
  }
}

const char* to_string(EventPhase phase) noexcept {
  switch (phase) {
    case EventPhase::Reset: return "Reset";
    case EventPhase::MediaCommit: return "MediaCommit";
    case EventPhase::FtlCommit: return "FtlCommit";
    case EventPhase::Ecc: return "Ecc";
    case EventPhase::Tsv: return "Tsv";
    case EventPhase::HostLinkComplete: return "HostLinkComplete";
    case EventPhase::CreditReturn: return "CreditReturn";
    case EventPhase::ControllerSchedule: return "ControllerSchedule";
    case EventPhase::HostSchedule: return "HostSchedule";
    case EventPhase::FinalDelivery: return "FinalDelivery";
  }
  return "InvalidEventPhase";
}

}  // namespace openhbf
