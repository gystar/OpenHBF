#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "openhbx/config/hbf_config_schema.h"
#include "openhbx/config/resolved_hbf_config.h"
#include "openhbx/integration/request_bridge.h"
#include "openhbx/system/open_hbx_system.h"

namespace {
std::string read_file(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot open ") + path);
  std::ostringstream bytes;
  bytes << input.rdbuf();
  return bytes.str();
}
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: openhbx_trace_runner CONFIG TRACE\n";
    return 2;
  }
  try {
    auto parsed = openhbx::config::parse_hbf_yaml(read_file(argv[1]));
    if (!parsed) throw std::runtime_error("invalid HBF YAML");
    auto resolved = openhbx::config::resolve_hbf_config(parsed.value());
    if (!resolved) throw std::runtime_error("HBF configuration rejected");
    auto built = openhbx::OpenHbxSystem::compose(resolved.take_value());
    if (!built) throw std::runtime_error(built.error);
    if (const char* resolved_path = std::getenv("OPENHBX_RESOLVED_CONFIG_OUT")) {
      std::ofstream resolved_output(resolved_path);
      if (!resolved_output) throw std::runtime_error("cannot write resolved config");
      resolved_output << built.system->resolved_config().canonical();
    }
    openhbx::integration::RequestBridge bridge(*built.system);

    std::ifstream trace(argv[2]);
    if (!trace) throw std::runtime_error("cannot open trace");
    std::uint64_t accepted = 0, completed = 0, source = 0;
    std::string line;
    while (std::getline(trace, line)) {
      if (line.empty() || line.front() == '#') continue;
      std::istringstream parser(line);
      char operation = 0;
      std::string address_text;
      int size = 0;
      unsigned pattern = 0;
      if (!(parser >> operation >> address_text >> size))
        throw std::runtime_error("invalid trace line: " + line);
      parser >> pattern;
      openhbx::integration::BridgeRequest request;
      request.address = static_cast<std::int64_t>(
          std::stoull(address_text, nullptr, 0));
      request.size_bytes = size;
      request.source_id = static_cast<std::int32_t>(source++);
      request.ingress_id = 0;
      request.type = operation == 'R'
          ? openhbx::integration::BridgeRequestType::Read
          : openhbx::integration::BridgeRequestType::Write;
      if (operation != 'R' && operation != 'W')
        throw std::runtime_error("invalid trace operation");
      if (operation == 'W')
        request.payload = openhbx::PayloadHandle::from_bytes(
            std::vector<std::uint8_t>(static_cast<std::size_t>(size),
                                      static_cast<std::uint8_t>(pattern)));
      request.completion = [&](const openhbx::SystemCompletion& completion) {
        ++completed;
        std::cout << completion.token.value() << ' '
                  << completion.completed_at.value() << ' '
                  << static_cast<unsigned>(completion.command_status) << ' '
                  << (completion.data_valid ? completion.payload.size() : 0) << '\n';
      };
      for (;;) {
        const auto result = bridge.try_submit(request);
        if (result.code == openhbx::AdmissionCode::Accepted) {
          ++accepted;
          break;
        }
        if (result.code == openhbx::AdmissionCode::Rejected)
          throw std::runtime_error("trace request rejected: " + result.detail);
        built.system->tick();
      }
    }
    const auto drained = built.system->drain(10000000);
    if (!drained.drained || accepted != completed)
      throw std::runtime_error("drain or completion conservation failed");
    if (const char* summary_path = std::getenv("OPENHBX_RUN_SUMMARY_OUT")) {
      const auto snapshot = built.system->snapshot();
      const auto stats = built.system->stats();
      std::ofstream summary(summary_path);
      if (!summary) throw std::runtime_error("cannot write run summary");
      summary << "{\n"
              << "  \"config_hash\": \""
              << built.system->resolved_config().canonical_hash() << "\",\n"
              << "  \"drained\": true,\n"
              << "  \"cycle\": " << snapshot.cycle.value() << ",\n"
              << "  \"generation\": " << snapshot.generation.value() << ",\n"
              << "  \"accepted\": " << snapshot.completions.accepted << ",\n"
              << "  \"terminal\": " << snapshot.completions.terminal << ",\n"
              << "  \"outstanding\": " << snapshot.completions.outstanding << ",\n"
              << "  \"events\": {\"queued\": " << snapshot.events.queued
              << ", \"scheduled\": " << snapshot.events.scheduled
              << ", \"dispatched\": " << snapshot.events.dispatched
              << ", \"stale\": " << snapshot.events.stale << "},\n"
              << "  \"stats\": {\"attempts\": " << stats.attempts
              << ", \"accepted\": " << stats.accepted
              << ", \"busy\": " << stats.busy
              << ", \"rejected\": " << stats.rejected
              << ", \"callbacks\": " << stats.callbacks
              << ", \"callback_errors\": " << stats.callback_errors
              << ", \"resets\": " << stats.resets
              << ", \"ticks\": " << stats.ticks
              << ", \"outstanding\": " << stats.outstanding
              << ", \"read_accepted_bytes\": " << stats.read_accepted_bytes
              << ", \"write_accepted_bytes\": " << stats.write_accepted_bytes
              << ", \"read_completed_requests\": " << stats.read_completed_requests
              << ", \"read_completed_bytes\": " << stats.read_completed_bytes
              << ", \"write_completed_requests\": " << stats.write_completed_requests
              << ", \"write_completed_bytes\": " << stats.write_completed_bytes
              << ", \"failed_requests\": " << stats.failed_requests
              << ", \"latency_samples\": " << stats.latency_samples
              << ", \"latency_sum_cycles\": " << stats.latency_sum_cycles
              << ", \"latency_min_cycles\": " << stats.latency_min_cycles
              << ", \"latency_max_cycles\": " << stats.latency_max_cycles
              << ", \"first_completion_cycle\": " << stats.first_completion_cycle
              << ", \"last_completion_cycle\": " << stats.last_completion_cycle
              << "},\n"
              << "  \"non_idle\": " << snapshot.non_idle.size() << "\n"
              << "}\n";
    }
    std::cerr << "config_hash=" << built.system->resolved_config().canonical_hash()
              << " accepted=" << accepted << " completed=" << completed
              << " cycles=" << built.system->cycle().value() << '\n';
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
