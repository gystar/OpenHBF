#include "openhbf/integration/ramulator_adapter.h"

#ifdef OPENHBF_WITH_RAMULATOR2

#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "ramulator/base/param.h"

namespace openhbf::integration {
namespace {

static_assert(Ramulator::Request::Type::Read == 0,
              "Ramulator Read request type changed");
static_assert(Ramulator::Request::Type::Write == 1,
              "Ramulator Write request type changed");

std::mutex factory_mutex;
RamulatorSystemFactory system_factory;
std::size_t live_adapters = 0;

std::uint16_t checked_u16(int value, const char* field) {
  if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
    throw std::invalid_argument(std::string(field) + "=" +
                                std::to_string(value) + " is outside uint16");
  }
  return static_cast<std::uint16_t>(value);
}

std::uint16_t checked_u16(unsigned value, const char* field) {
  if (value > std::numeric_limits<std::uint16_t>::max()) {
    throw std::invalid_argument(std::string(field) + "=" +
                                std::to_string(value) + " is outside uint16");
  }
  return static_cast<std::uint16_t>(value);
}

std::uint32_t checked_size(int value) {
  if (value <= 0) {
    throw std::invalid_argument("request.size_bytes=" + std::to_string(value) +
                                " must be positive");
  }
  return static_cast<std::uint32_t>(value);
}

}  // namespace

void install_ramulator_system_factory_for_process(
    RamulatorSystemFactory factory) {
  if (!factory) throw std::invalid_argument("Ramulator system factory is empty");
  std::lock_guard<std::mutex> lock(factory_mutex);
  if (system_factory) {
    throw std::logic_error("Ramulator system factory is already installed");
  }
  system_factory = std::move(factory);
}

void reset_ramulator_system_factory_for_test() {
  std::lock_guard<std::mutex> lock(factory_mutex);
  if (live_adapters != 0) {
    throw std::logic_error(
        "cannot reset Ramulator system factory while adapters are alive");
  }
  system_factory = {};
}

}  // namespace openhbf::integration

namespace Ramulator {

using namespace openhbf;
using namespace openhbf::integration;

void RamulatorMemorySystemAdapter::init() {
  RAMULATOR_PARSE_PARAM(config_.clock.ratio, std::uint32_t, "clock_ratio")
      .default_val(1);
  RAMULATOR_PARSE_PARAM(config_.clock.tck_picoseconds, std::uint32_t,
                        "tck_picoseconds")
      .default_val(1000);
  const unsigned channels = param<unsigned>("channels").default_val(1);
  const unsigned axi_interfaces =
      param<unsigned>("axi_interfaces_per_channel").default_val(1);
  config_.host.channels = checked_u16(channels, "channels");
  config_.host.axi_interfaces_per_channel =
      checked_u16(axi_interfaces, "axi_interfaces_per_channel");
  RAMULATOR_PARSE_PARAM(config_.host.queue_depth_per_channel, std::size_t,
                        "queue_depth_per_channel")
      .default_val(256);
  RAMULATOR_PARSE_PARAM(config_.host.transaction_bytes, std::uint32_t,
                        "transaction_bytes")
      .default_val(64);
  RAMULATOR_PARSE_PARAM(config_.controller.max_pending_dlus_per_channel,
                        std::size_t, "max_pending_dlus_per_channel")
      .default_val(64);
  RAMULATOR_PARSE_PARAM(config_.controller.accumulation_timeout_cycles,
                        std::uint64_t, "accumulation_timeout_cycles")
      .default_val(10000);
  const unsigned core_dies = param<unsigned>("core_dies").default_val(1);
  const unsigned dies_per_core =
      param<unsigned>("dies_per_core").default_val(1);
  const unsigned banks_per_die =
      param<unsigned>("banks_per_die").default_val(1);
  config_.geometry.core_dies = checked_u16(core_dies, "core_dies");
  config_.geometry.dies_per_core =
      checked_u16(dies_per_core, "dies_per_core");
  config_.geometry.banks_per_die =
      checked_u16(banks_per_die, "banks_per_die");
  RAMULATOR_PARSE_PARAM(config_.geometry.blocks_per_bank, std::uint32_t,
                        "blocks_per_bank")
      .default_val(1);
  RAMULATOR_PARSE_PARAM(config_.geometry.pages_per_block, std::uint32_t,
                        "pages_per_block")
      .default_val(1);
  RAMULATOR_PARSE_PARAM(config_.media_max_in_flight, std::size_t,
                        "media_max_in_flight")
      .default_val(256);
  RAMULATOR_PARSE_PARAM(config_.seed, std::uint64_t, "seed").default_val(1);
  const bool functional_payload =
      param<bool>("functional_payload").default_val(false);
  config_.payload_mode = functional_payload ? PayloadMode::Functional
                                            : PayloadMode::TimingOnly;
  config_.capabilities = config_.manifest();
  const auto valid = config_.validate();
  if (!valid) throw std::invalid_argument(valid.error().message);

  RamulatorSystemFactory factory;
  {
    std::lock_guard<std::mutex> lock(openhbf::integration::factory_mutex);
    factory = openhbf::integration::system_factory;
    if (factory) ++openhbf::integration::live_adapters;
  }
  if (!factory) {
    throw std::runtime_error(
        "production 01/03/04/05 pipeline factory unavailable");
  }
  try {
    system_ = factory(config_);
    if (!system_)
      throw std::runtime_error("Ramulator system factory returned null");
  } catch (...) {
    std::lock_guard<std::mutex> lock(openhbf::integration::factory_mutex);
    --openhbf::integration::live_adapters;
    throw;
  }

  m_stats.add("attempts", attempts_);
  m_stats.add("accepted", accepted_);
  m_stats.add("retries", retries_);
  m_stats.add("reads", reads_);
  m_stats.add("writes", writes_);
  m_stats.add("callbacks", *callbacks_);
}

RamulatorMemorySystemAdapter::~RamulatorMemorySystemAdapter() {
  if (!system_) return;
  std::lock_guard<std::mutex> lock(openhbf::integration::factory_mutex);
  if (openhbf::integration::live_adapters == 0) std::terminate();
  --openhbf::integration::live_adapters;
}

bool RamulatorMemorySystemAdapter::send(Ramulator::Request& request) {
  ++attempts_;
  if (request.type_id != Ramulator::Request::Type::Read &&
      request.type_id != Ramulator::Request::Type::Write) {
    throw std::invalid_argument("request.type_id=" +
                                std::to_string(request.type_id) +
                                " must be Read(0) or Write(1)");
  }
  if (request.addr < 0) {
    throw std::invalid_argument("request.addr must be non-negative");
  }
  if (config_.payload_mode == PayloadMode::Functional &&
      request.type_id == Ramulator::Request::Type::Write) {
    throw std::invalid_argument(
        "functional_payload=true requires explicit write bytes, which stock "
        "Ramulator::Request does not provide");
  }

  Ramulator::Request candidate = request;
  HostRequest host;
  host.opcode = request.type_id == Ramulator::Request::Type::Read
                    ? HostOpcode::Read
                    : HostOpcode::Write;
  host.address = static_cast<std::uint64_t>(request.addr);
  host.size_bytes = checked_size(request.size_bytes);
  host.channel = ChannelId(0);
  if (!request.addr_vec.empty()) {
    host.channel = ChannelId(checked_u16(request.addr_vec.front(),
                                        "request.addr_vec[0]"));
  }
  host.source_id = request.source_id;
  host.ingress_id = request.ingress_id;
  host.intra_channel_address = request.intra_channel_addr;
  host.address_vector.assign(request.addr_vec.begin(), request.addr_vec.end());
  host.arrive_cycle = request.arrive;
  host.axi_id = 0;
  const auto shape = validate(host, config_.host.channels,
                              config_.host.axi_interfaces_per_channel,
                              config_.payload_mode);
  if (!shape) throw std::invalid_argument(shape.error().message);

  auto callback_count = callbacks_;
  const auto outcome = system_->try_submit(
      std::move(host),
      [candidate = std::move(candidate), callback_count](
          const Completion& completion) mutable {
        if (completion.complete_cycle.value() >
            static_cast<std::uint64_t>(
                std::numeric_limits<Ramulator::Clk_t>::max())) {
          throw std::overflow_error("completion cycle exceeds Ramulator clock");
        }
        candidate.depart =
            static_cast<Ramulator::Clk_t>(completion.complete_cycle.value());
        ++*callback_count;
        if (candidate.callback) candidate.callback(candidate);
      });
  if (outcome.result == SubmitResult::Retry) {
    ++retries_;
    return false;
  }
  ++accepted_;
  if (request.type_id == Ramulator::Request::Type::Read) ++reads_;
  else ++writes_;
  return true;
}

void RamulatorMemorySystemAdapter::tick() { system_->tick(); }

int RamulatorMemorySystemAdapter::get_clock_ratio() {
  if (config_.clock.ratio > static_cast<std::uint32_t>(
                                std::numeric_limits<int>::max())) {
    throw std::overflow_error("clock_ratio exceeds Ramulator int API");
  }
  return static_cast<int>(config_.clock.ratio);
}

float RamulatorMemorySystemAdapter::get_tCK() {
  return static_cast<float>(config_.clock.tck_picoseconds) / 1000.0F;
}

int RamulatorMemorySystemAdapter::get_tx_bytes() {
  return static_cast<int>(config_.host.transaction_bytes);
}

void RamulatorMemorySystemAdapter::reset_stats() {
  attempts_ = accepted_ = retries_ = reads_ = writes_ = 0;
  *callbacks_ = 0;
}

}  // namespace Ramulator

#endif  // OPENHBF_WITH_RAMULATOR2
