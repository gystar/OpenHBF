#include <cstdlib>
#include <iostream>
#include "openhbx/media/nand/topology.h"

int main() {
  openhbx::config::HbfGeometry g{2, 2, 2, 3, 4, 8, 4096, 384, 1572864};
  openhbx::media::nand::MediaTopology topology(g);
  for (std::uint64_t i = 0; i < g.page_count; ++i) {
    const auto address = topology.decode_page(i);
    if (!address || topology.flatten_page(*address) != i) return EXIT_FAILURE;
  }
  if (topology.decode_page(g.page_count) ||
      topology.valid({g.core_dies, 0, 0, 0, 0})) return EXIT_FAILURE;
  if (topology.bank_id({1, 1, 2, 0, 0}) == topology.bank_id({1, 1, 1, 0, 0}))
    return EXIT_FAILURE;
  std::cout << "VER-MEDIA-001 topology round-trip passed\n";
  return EXIT_SUCCESS;
}
