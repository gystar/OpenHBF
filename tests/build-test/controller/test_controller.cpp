#include <cassert>
#include <array>
#include "openhbf/controller/dlu_accumulator.h"
#include "openhbf/controller/base_die_controller.h"
#include "openhbf/controller/backend.h"
#include "openhbf/controller/control_plane.h"

using namespace openhbf;
using namespace openhbf::controller;

namespace {
struct Host final : IHostResponsePort {
  std::vector<std::pair<HostToken, Status>> responses;
  bool enqueue(HostToken t, Status s) override { responses.emplace_back(t, s); return true; }
};
struct NullFtl final : IFtlControllerPort {
  IssueResult issue(const ReadyDlu&, Cycle) override { return {Status::Accepted, CommandToken(1)}; }
  void cancel(Generation) override {}
};
struct NullMedia final : IMediaControllerPort {
  IssueResult issue(const ReadyDlu&, Cycle) override { return {Status::Accepted, CommandToken(2)}; }
  void cancel(Generation) override {}
};
}

int main() {
  DluAccumulator acc(1, 2, Cycle(10));
  std::optional<ReadyDlu> ready;
  for (unsigned i = 0; i < 64; ++i) {
    WriteSegment s; s.channel = ChannelId(0); s.dlu = i == 0 ? 1 : 1;
    s.segment = static_cast<SegmentIndex>(i); s.host = HostToken(i + 1);
    s.generation = Generation(0); s.data[0] = static_cast<std::uint8_t>(i);
    assert(acc.accept(s, Cycle(0), &ready) == Status::Accepted);
  }
  assert(ready.has_value() && ready->data[0] == 0 && ready->data[63 * 64] == 63);

  WriteSegment duplicate; duplicate.channel = ChannelId(0); duplicate.dlu = 2;
  duplicate.segment = 0; duplicate.host = HostToken(100); duplicate.generation = Generation(0);
  assert(acc.accept(duplicate, Cycle(0), &ready) == Status::Accepted);
  assert(acc.accept(duplicate, Cycle(0), &ready) == Status::Duplicate);
  auto expired = acc.expire(Cycle(10));
  assert(expired.size() == 1 && expired[0].waiters.size() == 1);

  Host host; NullFtl ftl; NullMedia media;
  BaseDieController controller(1, 2, Cycle(10), host, ftl, media);
  WriteSegment segment; segment.channel = ChannelId(0); segment.host = HostToken(1);
  segment.generation = Generation(0); segment.segment = 0;
  assert(controller.accept(ChannelCommand{segment}, Cycle(0)) == Status::Accepted);
  controller.request_reset(Generation(1), Cycle(1));
  assert(host.responses.size() == 1 && host.responses[0].second == Status::Aborted);

  ControlPlane plane;
  AdminCommand command; command.opcode = AdminOpcode::GetLog;
  auto issue = plane.issue_admin(command, Cycle(0));
  assert(issue.kind == AdminIssue::Kind::Accepted);
  assert(plane.pop_admin().has_value());
  assert(plane.complete_admin(AdminCompletion{issue.token, plane.generation(), true, 0}));
  assert(!plane.complete_admin(AdminCompletion{issue.token, plane.generation(), true, 0}));
}
