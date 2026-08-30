#include "openhbx/hbf/address/hbf_geometry.h"

#include "openhbx/common/checked_math.h"
#include "openhbx/hbf/address/build_contract.h"

namespace openhbx::hbf::address {
const char* build_contract_domain() noexcept { return "hbf_address"; }

namespace {
bool fail(std::string* error, const char* message) {
  if (error != nullptr) *error = message;
  return false;
}
}  // namespace

std::optional<HbfGeometry> HbfGeometry::create(const GeometrySpec& spec,
                                               std::string* error) {
  if (spec.channels == 0 || spec.core_dies == 0 || spec.dies_per_core == 0 ||
      spec.banks_per_die == 0 || spec.blocks_per_bank == 0 || spec.r1 == 0 ||
      spec.r2 == 0 || spec.r3 == 0 || spec.r4 == 0 || spec.r5 == 0) {
    fail(error, "all geometry dimensions and R values must be non-zero");
    return std::nullopt;
  }
  const auto owned_banks = checked_mul(spec.r1, spec.r2);
  if (!owned_banks || spec.r5 != *owned_banks) {
    fail(error, "R5 must equal the checked R1*R2 bank stride");
    return std::nullopt;
  }
  const auto total_banks = checked_mul(spec.core_dies, spec.dies_per_core);
  const auto physical_banks = total_banks ? checked_mul(*total_banks, spec.banks_per_die)
                                          : std::nullopt;
  const auto assigned_banks = checked_mul(*owned_banks, spec.channels);
  if (!physical_banks || !assigned_banks || *physical_banks != *assigned_banks) {
    fail(error, "R1*R2*channels must equal the physical bank count");
    return std::nullopt;
  }
  auto units = checked_mul(*owned_banks, spec.blocks_per_bank);
  if (units) units = checked_mul(*units, spec.r3);
  if (units) units = checked_mul(*units, spec.r4);
  const auto bytes = units ? checked_mul(*units, 64) : std::nullopt;
  if (!bytes) {
    fail(error, "channel-local capacity overflows uint64");
    return std::nullopt;
  }
  return HbfGeometry(spec, *owned_banks, *bytes);
}

std::optional<HbfGeometry> HbfGeometry::from_config(
    const config::ResolvedHbfConfig& config, std::string* error) {
  const auto& value = config.geometry();
  auto total_banks = checked_mul(value.core_dies, value.dies_per_core);
  if (total_banks) total_banks = checked_mul(*total_banks, value.banks_per_die);
  if (!total_banks || value.host_channels == 0 ||
      *total_banks % value.host_channels != 0 || value.page_bytes % 64 != 0) {
    fail(error, "resolved geometry cannot be divided into channel bank slices");
    return std::nullopt;
  }
  const std::uint64_t owned = *total_banks / value.host_channels;
  GeometrySpec spec{value.host_channels, value.core_dies, value.dies_per_core,
                    value.banks_per_die, value.blocks_per_bank, owned, 1,
                    value.pages_per_block, value.page_bytes / 64, owned};
  return create(spec, error);
}

}  // namespace openhbx::hbf::address
