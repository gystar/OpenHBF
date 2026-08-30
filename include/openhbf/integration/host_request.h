#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "openhbf/common/error.h"
#include "openhbf/common/types.h"

namespace openhbf::integration {

enum class HostOpcode { Read, Write };
enum class HostStatus {
  Success,
  Aborted,
  InvalidRequest,
  ReadCecc,
  ReadUecc,
  ProgramFailed,
  InternalError,
};

using PayloadBytes = std::vector<std::uint8_t>;
using SharedPayload = std::shared_ptr<const PayloadBytes>;

struct HostRequest {
  // Zero at external ingress. OpenHbfSystem sets the Accepted token only after
  // admission and passes that owned copy to the injected endpoint.
  RequestToken token{};
  HostOpcode opcode = HostOpcode::Read;
  std::uint64_t address = 0;
  std::uint32_t size_bytes = 64;
  ChannelId channel{};
  std::uint16_t axi_id = 0;
  std::int32_t source_id = -1;
  std::int32_t ingress_id = -1;
  std::int64_t intra_channel_address = -1;
  std::vector<std::int64_t> address_vector;
  std::int64_t arrive_cycle = -1;
  SharedPayload payload;
};

struct Completion {
  RequestToken token{};
  Generation generation{};
  HostStatus status = HostStatus::InternalError;
  Cycle accepted_cycle{};
  std::optional<Cycle> issue_cycle;
  Cycle complete_cycle{};
  SharedPayload read_data;
};

using CompletionSink = std::function<void(const Completion&)>;

enum class PayloadMode { TimingOnly, Functional };
enum class SubmitResult { Accepted, Retry };
struct SubmitOutcome {
  SubmitResult result = SubmitResult::Retry;
  RequestToken token{};  // Non-zero if and only if result is Accepted.
};

Result<void> validate(const HostRequest& request, std::uint16_t channels,
                      std::uint16_t axi_interfaces_per_channel = 1,
                      PayloadMode payload_mode = PayloadMode::Functional);
const char* to_string(HostOpcode opcode) noexcept;
const char* to_string(HostStatus status) noexcept;
std::string describe(const HostRequest& request);

}  // namespace openhbf::integration
