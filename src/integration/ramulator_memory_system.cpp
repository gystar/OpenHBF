#include <cstdint>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/integration/request_bridge.h"
#include "openhbx/system/open_hbx_system.h"
#include "ramulator/base/base.h"
#include "ramulator/memory_system/i_memory_system.h"

namespace Ramulator {

class OpenHbxMemorySystem final : public IMemorySystem, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem, OpenHbxMemorySystem, "OpenHBX")

 public:
  void init() override {
    m_clock_ratio = param<int>("clock_ratio").required();
    if (m_clock_ratio <= 0) throw std::runtime_error("OpenHBX clock_ratio must be positive");

    openhbx::config::RawConfigTree raw;
    raw.scalars = {
        {"product.family", "HBF"},
        {"product.profile", "OCP_HBF_0_7"},
        {"geometry.source", m_config["geometry"]["source"].as<std::string>("synthetic")},
        {"geometry.host_channels", m_config["geometry"]["host_channels"].as<std::string>()},
        {"geometry.core_dies", m_config["geometry"]["core_dies"].as<std::string>()},
        {"geometry.dies_per_core", m_config["geometry"]["dies_per_core"].as<std::string>()},
        {"geometry.banks_per_die", m_config["geometry"]["banks_per_die"].as<std::string>()},
        {"geometry.blocks_per_bank", m_config["geometry"]["blocks_per_bank"].as<std::string>()},
        {"geometry.pages_per_block", m_config["geometry"]["pages_per_block"].as<std::string>()},
        {"geometry.page_bytes", m_config["geometry"]["page_bytes"].as<std::string>()},
        {"components.host", "OcpHbfTransactionLevel"},
        {"components.address", "HbfR1R5"},
        {"components.controller", "BaseDieFlash"},
        {"components.interconnect", "HbfTsvBaseline"},
        {"components.media", "NandFlash"},
        {"components.ras", "NandRawRas"},
        {"model.source", "ramulator:ConfigNode"},
        {"model.axi_interfaces", m_config["axi_interfaces"].as<std::string>("1")},
    };
    auto resolved = openhbx::config::resolve_hbf_config(raw);
    if (!resolved) throw std::runtime_error("OpenHBX Ramulator configuration is invalid");
    auto built = openhbx::OpenHbxSystem::compose(resolved.take_value());
    if (!built) throw std::runtime_error(built.error);
    m_system = std::move(built.system);
    m_bridge = std::make_unique<openhbx::integration::RequestBridge>(*m_system);

    m_stats.add("attempts", s_attempts);
    m_stats.add("accepted", s_accepted);
    m_stats.add("backpressured", s_backpressured);
    m_stats.add("callbacks", s_callbacks);
  }

  void setup(IFrontEnd*, IMemorySystem*) override {}

  bool send(Request& request) override {
    ++s_attempts;
    if (request.addr < 0 || request.size_bytes <= 0 || request.size_bytes > 4096 ||
        (request.type_id != Request::Type::Read && request.type_id != Request::Type::Write))
      throw std::runtime_error("invalid OpenHBX Ramulator Request");
    openhbx::integration::BridgeRequest bridge;
    bridge.address = request.addr;
    bridge.size_bytes = request.size_bytes;
    bridge.source_id = request.source_id < 0 ? 0 : request.source_id;
    bridge.ingress_id = request.ingress_id < 0 ? 0 : request.ingress_id;
    bridge.type = request.type_id == Request::Type::Read
        ? openhbx::integration::BridgeRequestType::Read
        : openhbx::integration::BridgeRequestType::Write;
    // Stock Ramulator Request carries no bytes. A deterministic placeholder
    // permits timing integration but is not functional write-data evidence.
    if (bridge.type == openhbx::integration::BridgeRequestType::Write)
      bridge.payload = openhbx::PayloadHandle::from_bytes(
          std::vector<std::uint8_t>(static_cast<std::size_t>(request.size_bytes), 0));
    auto owned = std::make_shared<Request>(request);
    bridge.completion = [this, owned = std::move(owned)](
                            const openhbx::SystemCompletion& completion) {
      if (completion.completed_at.value() >
          static_cast<std::uint64_t>(std::numeric_limits<Clk_t>::max()))
        throw std::overflow_error("OpenHBX completion cycle does not fit Ramulator Clk_t");
      owned->depart = static_cast<Clk_t>(completion.completed_at.value());
      ++s_callbacks;
      if (owned->callback) owned->callback(*owned);
    };
    const auto result = m_bridge->try_submit(std::move(bridge));
    if (result.code == openhbx::AdmissionCode::Accepted) {
      ++s_accepted;
      return true;
    }
    if (result.code == openhbx::AdmissionCode::Busy) {
      ++s_backpressured;
      return false;
    }
    throw std::runtime_error("OpenHBX rejected a valid Ramulator Request");
  }

  void tick() override { m_system->tick(); }
  int get_clock_ratio() override { return m_clock_ratio; }
  float get_tCK() override { return -1.0F; }
  int get_tx_bytes() override { return 64; }
  void finalize() override {
    const auto result = m_system->drain(1000000);
    if (!result.drained) throw std::runtime_error("OpenHBX finalize drain budget exhausted");
  }
  void update_stats() override {}
  void reset_stats() override {
    s_attempts = s_accepted = s_backpressured = s_callbacks = 0;
  }

 private:
  int m_clock_ratio{1};
  std::unique_ptr<openhbx::OpenHbxSystem> m_system;
  std::unique_ptr<openhbx::integration::RequestBridge> m_bridge;
  std::uint64_t s_attempts{0}, s_accepted{0}, s_backpressured{0}, s_callbacks{0};
};

}  // namespace Ramulator
