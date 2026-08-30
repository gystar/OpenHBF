#include <cstdlib>
#include <iostream>
#include <utility>

#include "openhbf/controller/base_die_controller.h"

using namespace openhbf;

namespace {
void check(bool value, const char* message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

class FakeBackend final : public IControllerBackend {
 public:
  bool program(const DluProgram& request, SimTime now,
               BackendCompletion completion) override {
    ++program_attempts;
    if (reject_next_program) { reject_next_program = false; return false; }
    ++programs; last_program = request; completion(Status::Success, nullptr, now + 10);
    return true;
  }
  bool read(const DluRead&, SimTime now, BackendCompletion completion) override {
    auto data = std::make_shared<ByteBuffer>(kDluBytes, 0xAB);
    completion(Status::Success, data, now + 5); return true;
  }
  int programs{};
  int program_attempts{};
  bool reject_next_program{};
  DluProgram last_program{};
};

Request write(RequestId id, std::uint64_t offset, std::uint32_t size,
              std::uint8_t value = 0x5A) {
  return {id, {0, offset}, 0, Operation::Write, size,
          std::make_shared<ByteBuffer>(size, value)};
}
}

int main() {
  HostConfig hc{1, 8192, 4, true, true};
  HostValidator validator(hc);
  check(validator.validate(write(1, 0, 64)).valid, "64B write valid");
  check(!validator.validate(write(2, 1, 64)).valid, "unaligned invalid");

  SameIdResponseOrderer orderer;
  Request a{10, {0, 0}, 2, Operation::Read, 64, nullptr};
  Request b{11, {0, 64}, 2, Operation::Read, 64, nullptr};
  orderer.admit(a); orderer.admit(b);
  check(orderer.complete({11, 0, 2, Operation::Read}).empty(), "same ID waits");
  auto ready = orderer.complete({10, 0, 2, Operation::Read});
  check(ready.size() == 2 && ready[0].id == 10 && ready[1].id == 11,
        "same ID ordered");

  FakeBackend backend;
  std::vector<Response> responses;
  BaseDieController controller({hc, 1, 20}, backend,
                               [&](Response r) { responses.push_back(std::move(r)); });
  check(controller.submit(write(20, 0, 64), 0), "first sector accepted");
  controller.submit(write(21, 0, 64), 1);
  check(responses.back().status == Status::OverlapAddress, "overlap 0x2");

  Request pending_read{22, {0, 64}, 1, Operation::Read, 64, nullptr};
  controller.submit(pending_read, 2);
  check(responses.back().status == Status::ReadFromPendingWrite, "pending read 0xA");
  Request forwarded{23, {0, 0}, 1, Operation::Read, 64, nullptr};
  controller.submit(forwarded, 2);
  check(responses.back().status == Status::Success && responses.back().data->at(0) == 0x5A,
        "received sector forwarded");

  controller.submit(write(24, 4096, 64), 3);
  check(responses.back().status == Status::PendingDluLimit, "pending limit 0x4");
  controller.tick(20);
  check(responses.back().id == 20 &&
        responses.back().status == Status::DluAccumulationTimeout, "timeout 0x5");

  responses.clear();
  for (std::uint32_t sector = 0; sector < kSectorsPerDlu; ++sector)
    controller.submit(write(100 + sector, sector * kSectorBytes, kSectorBytes,
                            static_cast<std::uint8_t>(sector)), 30 + sector);
  check(backend.programs == 1, "one backend program for complete DLU");
  check(responses.size() == kSectorsPerDlu, "non-posted completions after program");
  check(backend.last_program.data->at(63 * kSectorBytes) == 63, "DLU assembled");

  responses.clear();
  backend.reject_next_program = true;
  for (std::uint32_t sector = 0; sector < kSectorsPerDlu; ++sector)
    controller.submit(write(200 + sector, 4096 + sector * kSectorBytes,
                            kSectorBytes), 100 + sector);
  check(controller.pending_dlus(0) == 1, "backend busy retains complete DLU");
  controller.tick(200);
  check(controller.pending_dlus(0) == 0 && responses.size() == kSectorsPerDlu,
        "backend busy is retried");

  std::cout << "host/controller tests passed\n";
}
