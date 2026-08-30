#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "openhbx/config/resolved_hbf_config.h"

namespace openhbx::hbf::address {

struct GeometrySpec {
  std::uint64_t channels{0};
  std::uint64_t core_dies{0};
  std::uint64_t dies_per_core{0};
  std::uint64_t banks_per_die{0};
  std::uint64_t blocks_per_bank{0};
  std::uint64_t r1{0};
  std::uint64_t r2{0};
  std::uint64_t r3{0};
  std::uint64_t r4{0};
  std::uint64_t r5{0};
};

class HbfGeometry {
 public:
  static std::optional<HbfGeometry> create(const GeometrySpec& spec,
                                           std::string* error = nullptr);
  static std::optional<HbfGeometry> from_config(
      const config::ResolvedHbfConfig& config, std::string* error = nullptr);

  std::uint64_t channels() const noexcept { return spec_.channels; }
  std::uint64_t core_dies() const noexcept { return spec_.core_dies; }
  std::uint64_t dies_per_core() const noexcept { return spec_.dies_per_core; }
  std::uint64_t banks_per_die() const noexcept { return spec_.banks_per_die; }
  std::uint64_t blocks_per_bank() const noexcept { return spec_.blocks_per_bank; }
  std::uint64_t r1() const noexcept { return spec_.r1; }
  std::uint64_t r2() const noexcept { return spec_.r2; }
  std::uint64_t r3() const noexcept { return spec_.r3; }
  std::uint64_t r4() const noexcept { return spec_.r4; }
  std::uint64_t r5() const noexcept { return spec_.r5; }
  std::uint64_t owned_banks_per_channel() const noexcept { return owned_banks_; }
  std::uint64_t local_capacity_bytes() const noexcept { return local_capacity_bytes_; }
  const GeometrySpec& spec() const noexcept { return spec_; }

 private:
  HbfGeometry(GeometrySpec spec, std::uint64_t owned_banks,
              std::uint64_t local_capacity_bytes)
      : spec_(spec), owned_banks_(owned_banks),
        local_capacity_bytes_(local_capacity_bytes) {}
  GeometrySpec spec_;
  std::uint64_t owned_banks_;
  std::uint64_t local_capacity_bytes_;
};

}  // namespace openhbx::hbf::address
