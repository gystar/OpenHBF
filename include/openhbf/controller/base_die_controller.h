#pragma once
#include <unordered_map>
#include "backend.h"
#include "dlu_accumulator.h"
namespace openhbf::controller {
struct ChannelCommand { WriteSegment segment; };
class BaseDieController {
 public:
  BaseDieController(uint16_t channels,size_t max_dlu,Cycle timeout,IHostResponsePort& host,IFtlControllerPort& ftl,IMediaControllerPort& media);
  Status accept(ChannelCommand, Cycle);
  void on_event(const ControllerEvent&, Cycle);
  void request_reset(Generation, Cycle);
 private:
  DluAccumulator accumulator_; IHostResponsePort& host_; IFtlControllerPort& ftl_; IMediaControllerPort& media_; Generation generation_{};
  std::unordered_map<uint64_t,ControllerTxn> txns_;
  void complete(const ControllerTxn&,Status);
};
}
