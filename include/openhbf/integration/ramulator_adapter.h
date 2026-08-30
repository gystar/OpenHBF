#pragma once

#ifdef OPENHBF_WITH_RAMULATOR2

#include <cstdint>
#include <functional>
#include <memory>

#include "openhbf/integration/open_hbf_config.h"
#include "openhbf/integration/open_hbf_system.h"
#include "ramulator/memory_system/i_memory_system.h"

namespace openhbf::integration {

using RamulatorSystemFactory =
    std::function<std::unique_ptr<OpenHbfSystem>(const OpenHbfConfig&)>;

// Temporary process assembly hook until the production 01/03/04/05 pipeline
// has a canonical config factory. Installing twice is an error. Reset is
// intended for isolated tests and is rejected while an adapter is alive.
void install_ramulator_system_factory_for_process(RamulatorSystemFactory factory);
void reset_ramulator_system_factory_for_test();

}  // namespace openhbf::integration

namespace Ramulator {

class RamulatorMemorySystemAdapter final : public IMemorySystem,
                                           public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem,
                                    RamulatorMemorySystemAdapter, "OpenHBF")

 public:
  ~RamulatorMemorySystemAdapter() override;

  void init() override;
  bool send(Ramulator::Request& request) override;
  void tick() override;
  int get_clock_ratio() override;
  float get_tCK() override;
  int get_tx_bytes() override;
  void reset_stats() override;

 private:
  openhbf::integration::OpenHbfConfig config_;
  std::unique_ptr<openhbf::integration::OpenHbfSystem> system_;
  std::uint64_t attempts_ = 0;
  std::uint64_t accepted_ = 0;
  std::uint64_t retries_ = 0;
  std::uint64_t reads_ = 0;
  std::uint64_t writes_ = 0;
  std::shared_ptr<std::uint64_t> callbacks_ =
      std::make_shared<std::uint64_t>(0);
};

}  // namespace Ramulator

#endif  // OPENHBF_WITH_RAMULATOR2
