#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <deque>
#include <unordered_map>

#include "openhbf/common/error.h"
#include "openhbf/common/types.h"

namespace openhbf::controller {

enum class LifecycleState : std::uint8_t { PowerOff, Por, LinkWait, Ready, Throttled, Resetting, Cattrip, Fault };
enum class ResetKind : std::uint8_t { Soft, Channel, PowerCycle };
enum class AdminOpcode : std::uint8_t { GetFeature, SetFeature, SecureErase, GetLog, Bist, ZoneRemap, ReadUcieErrors, ReducedCapacity };
enum class Register : std::uint16_t { Buccap = 0x0000, Bucc = 0x000c, Bucsts = 0x0010, Bucr = 0x0014, Tmon = 0x0018 };

struct AdminCommand { AdminOpcode opcode{}; std::uint64_t argument{}; Generation generation{}; };
struct AdminEntry { RequestToken token{}; AdminCommand command{}; };
struct AdminCompletion { RequestToken token{}; Generation generation{}; bool success{}; std::uint32_t status{}; };
struct AdminIssue { enum class Kind { Accepted, Busy, Rejected }; Kind kind{}; RequestToken token{}; std::uint32_t status{}; };

class RegisterBank {
 public:
  explicit RegisterBank(std::uint32_t buccap = 0);
  Result<std::uint32_t> read(Register reg) const;
  Result<void> write(Register reg, std::uint32_t value);
  void set_ready(bool ready) noexcept;
  void set_throttled(bool throttled) noexcept;
  void set_cattrip(bool active) noexcept;
  std::uint32_t tmon() const noexcept { return tmon_; }
 private:
  std::uint32_t buccap_, bucc_ = 0, bucsts_ = 0, bucr_ = 0, tmon_ = 0;
};

class LifecycleFsm {
 public:
  using GenerationSink = std::function<void(Generation)>;
  LifecycleFsm();
  LifecycleState state() const noexcept { return state_; }
  Generation generation() const noexcept { return generation_; }
  void set_generation_sink(GenerationSink sink) { sink_ = std::move(sink); }
  void set_dependencies(bool host, bool media, bool tsv, bool ftl);
  void set_enable(bool enabled);
  void set_temperature(std::int32_t celsius, std::int32_t throttle, std::int32_t trip);
  void reset(ResetKind kind);
 private:
  void bump_generation();
  LifecycleState state_ = LifecycleState::PowerOff;
  Generation generation_{0};
  bool host_ = false, media_ = false, tsv_ = false, ftl_ = false, enabled_ = false;
  GenerationSink sink_;
};

class AdminEngine {
 public:
  explicit AdminEngine(std::size_t depth = 16);
  AdminIssue issue(AdminCommand command, Cycle now);
  // Moves one command to the in-flight set and preserves its token.
  std::optional<AdminEntry> pop();
  bool complete(const AdminCompletion& completion);
  void cancel(Generation generation);
 private:
  std::size_t depth_; RequestToken next_{1};
  std::deque<AdminEntry> queue_;
  std::unordered_map<std::uint64_t, AdminEntry> inflight_;
};

class ControlPlane {
 public:
  using EventSchedule = std::function<void(Cycle, Generation, RequestToken)>;
  explicit ControlPlane(std::uint32_t buccap = 0, std::size_t admin_depth = 16);
  Result<std::uint32_t> read_reg(Register reg) const { return regs_.read(reg); }
  Result<void> write_reg(Register reg, std::uint32_t value);
  AdminIssue issue_admin(AdminCommand command, Cycle now);
  std::optional<AdminEntry> pop_admin() { return admin_.pop(); }
  bool complete_admin(const AdminCompletion& completion);
  void set_dependencies(bool host, bool media, bool tsv, bool ftl);
  void set_temperature(std::int32_t celsius, std::int32_t throttle, std::int32_t trip);
  void request_reset(ResetKind kind);
  LifecycleState state() const noexcept { return fsm_.state(); }
  Generation generation() const noexcept { return fsm_.generation(); }
  void set_event_schedule(EventSchedule schedule) { schedule_ = std::move(schedule); }
 private:
  RegisterBank regs_; LifecycleFsm fsm_; AdminEngine admin_; EventSchedule schedule_;
};
}  // namespace openhbf::controller
