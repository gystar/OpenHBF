#include "openhbx/media/nand/topology.h"

#include <limits>

namespace openhbx::media::nand {
MediaTopology::MediaTopology(config::HbfGeometry geometry) : geometry_(geometry) {}

bool MediaTopology::valid(const PhysicalAddress& a) const noexcept {
  return a.core_die < geometry_.core_dies && a.die < geometry_.dies_per_core &&
         a.bank < geometry_.banks_per_die && a.block < geometry_.blocks_per_bank &&
         a.page < geometry_.pages_per_block && geometry_.page_bytes == 4096;
}

std::optional<std::uint64_t> MediaTopology::flatten_page(const PhysicalAddress& a) const noexcept {
  if (!valid(a)) return std::nullopt;
  std::uint64_t v = a.core_die;
  v = v * geometry_.dies_per_core + a.die;
  v = v * geometry_.banks_per_die + a.bank;
  v = v * geometry_.blocks_per_bank + a.block;
  return v * geometry_.pages_per_block + a.page;
}

std::optional<PhysicalAddress> MediaTopology::decode_page(std::uint64_t value) const noexcept {
  if (value >= geometry_.page_count || geometry_.pages_per_block == 0 ||
      geometry_.blocks_per_bank == 0 || geometry_.banks_per_die == 0 ||
      geometry_.dies_per_core == 0) return std::nullopt;
  PhysicalAddress a;
  a.page = value % geometry_.pages_per_block; value /= geometry_.pages_per_block;
  a.block = value % geometry_.blocks_per_bank; value /= geometry_.blocks_per_bank;
  a.bank = value % geometry_.banks_per_die; value /= geometry_.banks_per_die;
  a.die = value % geometry_.dies_per_core; value /= geometry_.dies_per_core;
  a.core_die = value;
  return valid(a) ? std::optional<PhysicalAddress>(a) : std::nullopt;
}

std::optional<std::uint64_t> MediaTopology::flatten_block(const PhysicalAddress& a) const noexcept {
  auto page = flatten_page(a);
  if (!page) return std::nullopt;
  return *page / geometry_.pages_per_block;
}
std::uint64_t MediaTopology::bank_id(const PhysicalAddress& a) const noexcept {
  return (a.core_die * geometry_.dies_per_core + a.die) * geometry_.banks_per_die + a.bank;
}
std::uint64_t MediaTopology::die_id(const PhysicalAddress& a) const noexcept {
  return a.core_die * geometry_.dies_per_core + a.die;
}
}  // namespace openhbx::media::nand
