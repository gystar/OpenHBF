#pragma once

#include <cstdint>
#include <map>

namespace openhbx::ras {
enum class RawReadClass { Clean, Correctable, Uncorrectable, Retry, Refresh };
enum class ForcedFailure { None, Program, Erase };
struct ReliabilityProfile {
  std::uint64_t seed{0};
  std::uint64_t refresh_read_threshold{0};
};
class ReliabilityModel {
 public:
  explicit ReliabilityModel(ReliabilityProfile profile) : profile_(profile) {}
  RawReadClass evaluate_read(std::uint64_t page, std::uint64_t pec, double temperature_c);
  void force_read(std::uint64_t page, RawReadClass outcome) { forced_reads_[page] = outcome; }
  void force_failure(std::uint64_t block, ForcedFailure failure) { failures_[block] = failure; }
  bool fails(std::uint64_t block, ForcedFailure failure) const;
  void clear_forced_failure(std::uint64_t block) { failures_.erase(block); }
 private:
  ReliabilityProfile profile_;
  std::map<std::uint64_t, std::uint64_t> reads_;
  std::map<std::uint64_t, RawReadClass> forced_reads_;
  std::map<std::uint64_t, ForcedFailure> failures_;
};
}  // namespace openhbx::ras
