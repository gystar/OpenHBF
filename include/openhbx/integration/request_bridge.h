#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "openhbx/system/open_hbx_system.h"

namespace openhbx::integration {

enum class BridgeRequestType { Read, Write };

struct BridgeRequest {
  std::int64_t address{-1};
  std::int32_t size_bytes{-1};
  std::int32_t source_id{-1};
  std::int32_t ingress_id{-1};
  BridgeRequestType type{BridgeRequestType::Read};
  PayloadHandle payload;
  std::function<void(const SystemCompletion&)> completion;
};

class RequestBridge {
 public:
  explicit RequestBridge(OpenHbxSystem& system);
  AdmissionResult try_submit(BridgeRequest request);
  std::uint64_t next_token() const noexcept { return next_token_; }

 private:
  OpenHbxSystem& system_;
  std::uint64_t next_token_{0};
};

}  // namespace openhbx::integration
