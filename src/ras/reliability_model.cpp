#include "openhbx/ras/reliability_model.h"
namespace openhbx::ras {
RawReadClass ReliabilityModel::evaluate_read(std::uint64_t page, std::uint64_t pec,
                                              double temperature_c) {
  const auto forced = forced_reads_.find(page);
  if (forced != forced_reads_.end()) return forced->second;
  const auto count = ++reads_[page];
  if (profile_.refresh_read_threshold != 0 && count >= profile_.refresh_read_threshold)
    return RawReadClass::Refresh;
  // Stable mixing provides deterministic synthetic raw degradation without host entropy.
  const std::uint64_t mixed = (page * 0x9e3779b97f4a7c15ULL) ^ profile_.seed ^
                              (pec * 0xbf58476d1ce4e5b9ULL) ^
                              static_cast<std::uint64_t>(temperature_c * 10.0);
  return (mixed % 1000003ULL == 0 && pec != 0) ? RawReadClass::Correctable : RawReadClass::Clean;
}
bool ReliabilityModel::fails(std::uint64_t block, ForcedFailure failure) const {
  const auto it = failures_.find(block);
  return it != failures_.end() && it->second == failure;
}
}  // namespace openhbx::ras
