#pragma once

#include <cstdint>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/media/eat.h"
#include "openhbf/media/types.h"

namespace openhbf::media {

// Temperature is expressed in degrees Celsius. It selects policy but never
// advances simulated time or samples a fault.
using Temperature = double;

struct TimingSelection {
  std::vector<StageSpec> stages;
  bool synthetic = true;
};

class NandTimingModel {
 public:
  static Result<NandTimingModel> make_synthetic_defaults(
      CellMode mode, std::uint64_t simulation_cycle_ns = 1);

  Result<TimingSelection> stages_for(MediaOp op, const MediaAddress& address,
                                     PageClass page_class,
                                     Temperature temperature) const;
  Result<void> validate() const;
  CellMode cell_mode() const noexcept { return mode_; }
  std::uint64_t simulation_cycle_ns() const noexcept { return cycle_ns_; }
  const char* provenance() const noexcept;

 private:
  NandTimingModel(CellMode mode, std::uint64_t cycle_ns)
      : mode_(mode), cycle_ns_(cycle_ns) {}

  CellMode mode_ = CellMode::Slc;
  std::uint64_t cycle_ns_ = 1;
};

}  // namespace openhbf::media
