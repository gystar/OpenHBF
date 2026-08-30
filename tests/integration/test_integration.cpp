#include "openhbf/integration/integration_engine.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace openhbf::integration;

int main() {
  Profile profile;
  profile.queue_depth = 128;
  profile.read_latency_cycles = 3;
  profile.program_latency_cycles = 5;
  IntegrationEngine engine(profile);
  std::vector<Completion> completions;

  for (std::uint64_t sector = 0; sector < 64; ++sector) {
    Request request;
    request.address = sector * 64;
    request.type = RequestType::Write;
    request.size_bytes = 64;
    request.callback = [&completions](const Completion& completion) { completions.push_back(completion); };
    assert(engine.send(request));
  }
  for (int i = 0; i < 4; ++i) engine.tick();
  assert(completions.empty());
  engine.tick();
  assert(completions.size() == 64);
  for (const auto& completion : completions) {
    assert(completion.depart >= completion.arrive);
    assert(completion.command_status == 0);
  }
  assert(engine.stats().programmed_dlus == 1);

  Request read;
  read.address = 0;
  read.type = RequestType::Read;
  read.size_bytes = 64;
  read.source_id = 7;
  read.ingress_id = 3;
  read.callback = [&completions](const Completion& completion) { completions.push_back(completion); };
  assert(engine.send(read));
  while (!engine.idle()) engine.tick();
  assert(completions.size() == 65);
  assert(completions.back().source_id == 7);
  assert(completions.back().ingress_id == 3);
  assert(engine.stats().accepted == engine.stats().completed);
  assert(engine.stats().callbacks == engine.stats().completed);

  Profile blocked_profile;
  blocked_profile.queue_depth = 1;
  IntegrationEngine blocked(blocked_profile);
  Request pending = read;
  assert(blocked.send(pending));
  assert(!blocked.send(pending));
  assert(blocked.stats().accepted == 1);
  while (!blocked.idle()) blocked.tick();

  std::cout << "integration tests passed\n";
}
