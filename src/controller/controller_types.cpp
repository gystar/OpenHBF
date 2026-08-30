#include "openhbf/controller/controller_types.h"
namespace openhbf::controller {
const char* to_string(Status s) noexcept { switch(s){case Status::Accepted:return "accepted";case Status::Busy:return "busy";case Status::Completed:return "completed";case Status::Aborted:return "aborted";case Status::Invalid:return "invalid";case Status::Duplicate:return "duplicate";case Status::Mpdlu:return "mpdlu";case Status::Timeout:return "timeout";case Status::MissingSegment:return "missing_segment";} return "unknown"; }
bool valid_transition(ControllerState a, ControllerState b) noexcept { if(a==ControllerState::Completed||a==ControllerState::Failed||a==ControllerState::Aborted)return false; return b==ControllerState::Completed||b==ControllerState::Failed||b==ControllerState::Aborted||static_cast<int>(b)>=static_cast<int>(a); }
}
