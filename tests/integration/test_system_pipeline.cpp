#include <cassert>
#include <iostream>
#include <vector>

#include "openhbf/controller/base_die_controller.h"
#include "openhbf/controller/ftl_media_backend.h"

int main() {
  openhbf::media::NandMedia media({1, 1, 1, 2, 4}, {3, 5, 7, 1});
  openhbf::ftl::SequentialMapper mapper({1, 1, 1, 4, 64, 1, 2}, media);
  openhbf::FtlMediaBackend backend(mapper);
  std::vector<openhbf::Response> responses;
  openhbf::ControllerConfig config;
  config.host.channel_capacity_bytes = 8 * openhbf::kDluBytes;
  openhbf::BaseDieController controller(
      config, backend,
      [&](openhbf::Response response) { responses.push_back(std::move(response)); });

  for (std::uint64_t sector = 0; sector < openhbf::kSectorsPerDlu; ++sector) {
    auto data = std::make_shared<openhbf::ByteBuffer>(openhbf::kSectorBytes,
                                                     static_cast<std::uint8_t>(sector));
    assert(controller.submit({sector + 1, {0, sector * openhbf::kSectorBytes},
                              0, openhbf::Operation::Write,
                              openhbf::kSectorBytes, data}, 0));
  }
  assert(responses.empty());
  for (openhbf::SimTime now = 1; now < 20; ++now) {
    backend.tick(now);
    controller.tick(now);
  }
  assert(responses.size() == openhbf::kSectorsPerDlu);
  for (const auto& response : responses) {
    assert(response.status == openhbf::Status::Success);
    assert(response.completed_at == 12);  // page-0 erase plus program
  }

  responses.clear();
  assert(controller.submit({100, {0, 0}, 0, openhbf::Operation::Read,
                            openhbf::kSectorBytes, nullptr}, 20));
  backend.tick(23);
  assert(responses.size() == 1);
  assert(responses.front().status == openhbf::Status::Success);
  assert(responses.front().data && responses.front().data->size() == 64);
  assert(backend.idle());

  responses.clear();
  const auto page = mapper.map_dlu(0, 0);
  media.inject_fault({openhbf::media::Operation::kRead, page,
                      openhbf::media::Status::kUecc, 0, true});
  assert(controller.submit({101, {0, 0}, 0, openhbf::Operation::Read,
                            openhbf::kSectorBytes, nullptr}, 30));
  backend.tick(33);
  assert(responses.size() == 1);
  assert(responses.front().status ==
         openhbf::Status::ReadUeccRefreshRequired);
  std::cout << "system pipeline tests passed\n";
}
