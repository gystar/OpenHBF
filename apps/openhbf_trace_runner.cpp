#include "openhbf/integration/integration_engine.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using openhbf::integration::Completion;
using openhbf::integration::IntegrationEngine;
using openhbf::integration::Request;
using openhbf::integration::RequestType;

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: openhbf_trace_runner TRACE [PROFILE]\n";
    return 2;
  }
  try {
    IntegrationEngine engine(argc == 3 ? openhbf::integration::load_profile(argv[2])
                                       : openhbf::integration::Profile{});
    std::ifstream trace(argv[1]);
    if (!trace) throw std::runtime_error("cannot open trace");
    std::uint64_t completions = 0;
    std::string line;
    while (std::getline(trace, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream parser(line);
      char op;
      std::string address;
      int size;
      if (!(parser >> op >> address >> size)) throw std::runtime_error("invalid trace line: " + line);
      Request request;
      request.address = std::stoull(address, nullptr, 0);
      request.type = op == 'R' ? RequestType::Read : op == 'W' ? RequestType::Write
                                                               : throw std::runtime_error("invalid trace op");
      request.size_bytes = size;
      request.callback = [&completions](const Completion& result) {
        ++completions;
        std::cout << result.sequence << ' ' << result.depart << ' '
                  << static_cast<int>(result.command_status) << '\n';
      };
      while (!engine.send(request)) engine.tick();
    }
    std::uint64_t drain = 0;
    while (!engine.idle()) {
      if (++drain > 10000000) throw std::runtime_error("drain timeout");
      engine.tick();
    }
    const auto& stats = engine.stats();
    if (completions != stats.accepted || stats.completed != stats.accepted) {
      throw std::runtime_error("completion accounting mismatch");
    }
    std::cerr << "accepted=" << stats.accepted << " completed=" << stats.completed
              << " cycles=" << engine.clock() << " programmed_dlus=" << stats.programmed_dlus << '\n';
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
