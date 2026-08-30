#include "openhbf/controller/control_plane.h"

#include <limits>

namespace openhbf::controller {
namespace {
constexpr std::uint32_t kEnable = 1u;
constexpr std::uint32_t kReady = 1u;
constexpr std::uint32_t kCes = 1u << 0;
constexpr std::uint32_t kCattrip = 1u << 1;
constexpr std::uint32_t kBucrResetMagic = 0x52455345u;
Error err(ErrorCode code, const char* message) { return Error{code, message}; }
}

RegisterBank::RegisterBank(std::uint32_t buccap) : buccap_(buccap) {}
Result<std::uint32_t> RegisterBank::read(Register reg) const {
  switch (reg) {
    case Register::Buccap: return Result<std::uint32_t>::success(buccap_);
    case Register::Bucc: return Result<std::uint32_t>::success(bucc_);
    case Register::Bucsts: return Result<std::uint32_t>::success(bucsts_);
    case Register::Bucr: return Result<std::uint32_t>::success(bucr_);
    case Register::Tmon: return Result<std::uint32_t>::success(tmon_);
  }
  return Result<std::uint32_t>::failure(err(ErrorCode::kOutOfRange, "unknown register"));
}
Result<void> RegisterBank::write(Register reg, std::uint32_t value) {
  switch (reg) {
    case Register::Bucc: bucc_ = value & kEnable; return Result<void>::success();
    case Register::Bucr:
      if (value != kBucrResetMagic)
        return Result<void>::failure(err(ErrorCode::kInvalidArgument, "invalid BUCR command"));
      bucr_ = value;
      return Result<void>::success();
    case Register::Buccap: case Register::Bucsts: case Register::Tmon:
      return Result<void>::failure(err(ErrorCode::kUnsupported, "read-only register"));
  }
  return Result<void>::failure(err(ErrorCode::kOutOfRange, "unknown register"));
}
void RegisterBank::set_ready(bool ready) noexcept { if (ready) bucsts_ |= kReady; else bucsts_ &= ~kReady; }
void RegisterBank::set_throttled(bool throttled) noexcept { if (throttled) bucsts_ |= (1u << 1); else bucsts_ &= ~(1u << 1); }
void RegisterBank::set_cattrip(bool active) noexcept {
  if (active) {
    // CATTRIP and CES are sticky until an explicit lifecycle recovery/reset.
    tmon_ |= kCattrip | kCes;
  } else {
    tmon_ &= ~kCattrip;
  }
}

LifecycleFsm::LifecycleFsm() = default;
void LifecycleFsm::bump_generation() { generation_ = Generation(generation_.value() + 1); if (sink_) sink_(generation_); }
void LifecycleFsm::set_dependencies(bool host, bool media, bool tsv, bool ftl) {
  host_ = host; media_ = media; tsv_ = tsv; ftl_ = ftl;
  if (host_ && media_ && tsv_ && ftl_ && enabled_) state_ = LifecycleState::Ready;
}
void LifecycleFsm::set_enable(bool enabled) { enabled_ = enabled; if (enabled_ && host_ && media_ && tsv_ && ftl_) state_ = LifecycleState::Ready; else if (!enabled_) state_ = LifecycleState::LinkWait; }
void LifecycleFsm::set_temperature(std::int32_t celsius, std::int32_t throttle, std::int32_t trip) {
  if (celsius >= trip) { state_ = LifecycleState::Cattrip; bump_generation(); }
  else if (celsius >= throttle && state_ == LifecycleState::Ready) state_ = LifecycleState::Throttled;
  else if (state_ == LifecycleState::Throttled && celsius < throttle) state_ = LifecycleState::Ready;
}
void LifecycleFsm::reset(ResetKind kind) { state_ = kind == ResetKind::PowerCycle ? LifecycleState::Por : LifecycleState::Resetting; bump_generation(); if (kind == ResetKind::PowerCycle) { host_ = media_ = tsv_ = ftl_ = enabled_ = false; } }

AdminEngine::AdminEngine(std::size_t depth) : depth_(depth) {}
AdminIssue AdminEngine::issue(AdminCommand command, Cycle) {
  if (queue_.size() + inflight_.size() >= depth_)
    return {AdminIssue::Kind::Busy, {}, 2};
  const RequestToken token(next_.value());
  next_ = RequestToken(next_.value() + 1);
  queue_.push_back(AdminEntry{token, command});
  return {AdminIssue::Kind::Accepted, token, 0};
}
std::optional<AdminEntry> AdminEngine::pop() {
  if (queue_.empty()) return std::nullopt;
  AdminEntry entry = queue_.front();
  queue_.pop_front();
  inflight_.emplace(entry.token.value(), entry);
  return entry;
}
bool AdminEngine::complete(const AdminCompletion& completion) {
  if (!is_valid_token(completion.token)) return false;
  auto it = inflight_.find(completion.token.value());
  if (it == inflight_.end() || it->second.command.generation != completion.generation) return false;
  inflight_.erase(it);
  return true;
}
void AdminEngine::cancel(Generation generation) {
  for (auto it = queue_.begin(); it != queue_.end();) {
    if (it->command.generation == generation) it = queue_.erase(it);
    else ++it;
  }
  for (auto it = inflight_.begin(); it != inflight_.end();) {
    if (it->second.command.generation == generation) it = inflight_.erase(it);
    else ++it;
  }
}

ControlPlane::ControlPlane(std::uint32_t buccap, std::size_t admin_depth) : regs_(buccap), admin_(admin_depth) {}
Result<void> ControlPlane::write_reg(Register reg, std::uint32_t value) {
  auto result = regs_.write(reg, value);
  if (result && reg == Register::Bucr && value == kBucrResetMagic) request_reset(ResetKind::Soft);
  if (result && reg == Register::Bucc) fsm_.set_enable((value & kEnable) != 0);
  return result;
}
AdminIssue ControlPlane::issue_admin(AdminCommand command, Cycle now) { if (fsm_.state() == LifecycleState::Cattrip || fsm_.state() == LifecycleState::Resetting) return {AdminIssue::Kind::Busy, {}, 3}; command.generation = generation(); auto result = admin_.issue(command, now); if (result.kind == AdminIssue::Kind::Accepted && schedule_) schedule_(now, generation(), result.token); return result; }
bool ControlPlane::complete_admin(const AdminCompletion& completion) { return completion.generation == generation() && admin_.complete(completion); }
void ControlPlane::set_dependencies(bool host, bool media, bool tsv, bool ftl) { fsm_.set_dependencies(host, media, tsv, ftl); regs_.set_ready(fsm_.state() == LifecycleState::Ready); }
void ControlPlane::set_temperature(std::int32_t celsius, std::int32_t throttle, std::int32_t trip) { fsm_.set_temperature(celsius, throttle, trip); regs_.set_throttled(fsm_.state() == LifecycleState::Throttled); if (fsm_.state() == LifecycleState::Cattrip) regs_.set_cattrip(true); }
void ControlPlane::request_reset(ResetKind kind) { const Generation old = generation(); admin_.cancel(old); fsm_.reset(kind); regs_.set_ready(false); regs_.set_cattrip(false); }
}  // namespace openhbf::controller
