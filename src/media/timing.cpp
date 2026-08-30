#include "openhbf/media/timing.h"

#include <cmath>
#include <limits>
#include <utility>

#include "openhbf/common/checked_math.h"

namespace openhbf::media {
namespace {

// OpenHBF synthetic defaults. OCP HBF v0.7.0 does not specify NAND tR,
// tPROG or tERS. Values below are deterministic modeling inputs, not OCP
// requirements or guarantees. All fields are nanoseconds.
struct RawTiming {
  std::uint64_t read_sense;
  std::uint64_t read_out;
  std::uint64_t program_in;
  std::uint64_t program_array;
  std::uint64_t program_verify;
  std::uint64_t erase_array;
  std::uint64_t erase_verify;
};

constexpr RawTiming kSlc{25000, 4000, 4000, 200000, 20000, 1500000, 100000};
constexpr RawTiming kMlc{50000, 4000, 4000, 600000, 40000, 2000000, 120000};
constexpr RawTiming kTlcLsb{60000, 4000, 4000, 700000, 50000, 2500000, 150000};
constexpr RawTiming kTlcCsb{85000, 4000, 4000, 1100000, 70000, 2500000, 150000};
constexpr RawTiming kTlcMsb{110000, 4000, 4000, 1500000, 90000, 2500000, 150000};
constexpr RawTiming kQlc{140000, 4000, 4000, 2200000, 120000, 3000000, 180000};

Result<Duration> to_cycles(std::uint64_t nanoseconds, std::uint64_t cycle_ns) {
  auto converted = checked_ceil_div(nanoseconds, cycle_ns);
  if (!converted) {
    return Result<Duration>::failure(converted.error());
  }
  if (converted.value() == 0) {
    return Result<Duration>::failure(
        {ErrorCode::kInvalidArgument, "timing duration rounds to zero cycles"});
  }
  return Result<Duration>::success(Duration(converted.value()));
}

Result<const RawTiming*> select_timing(CellMode mode, PageClass page_class) {
  switch (mode) {
    case CellMode::Slc:
      if (page_class != PageClass::Default) break;
      return Result<const RawTiming*>::success(&kSlc);
    case CellMode::Mlc:
      // The synthetic MLC profile is intentionally one explicit class. It
      // never guesses a TLC LSB/CSB/MSB mapping.
      if (page_class != PageClass::Default) break;
      return Result<const RawTiming*>::success(&kMlc);
    case CellMode::Tlc:
      if (page_class == PageClass::Lsb)
        return Result<const RawTiming*>::success(&kTlcLsb);
      if (page_class == PageClass::Csb)
        return Result<const RawTiming*>::success(&kTlcCsb);
      if (page_class == PageClass::Msb)
        return Result<const RawTiming*>::success(&kTlcMsb);
      break;
    case CellMode::Qlc:
      // QLC layout is vendor-specific. Default names an explicit aggregate
      // synthetic class; no page-index modulo classification is inferred.
      if (page_class != PageClass::Default) break;
      return Result<const RawTiming*>::success(&kQlc);
  }
  return Result<const RawTiming*>::failure(
      {ErrorCode::kUnsupported,
       "page class is not defined for the selected cell mode"});
}

}  // namespace

Result<NandTimingModel> NandTimingModel::make_synthetic_defaults(
    CellMode mode, std::uint64_t simulation_cycle_ns) {
  NandTimingModel model(mode, simulation_cycle_ns);
  auto valid = model.validate();
  if (!valid) {
    return Result<NandTimingModel>::failure(valid.error());
  }
  return Result<NandTimingModel>::success(std::move(model));
}

Result<void> NandTimingModel::validate() const {
  if (cycle_ns_ == 0) {
    return Result<void>::failure(
        {ErrorCode::kInvalidArgument, "simulation cycle must be non-zero ns"});
  }
  switch (mode_) {
    case CellMode::Slc:
    case CellMode::Mlc:
    case CellMode::Tlc:
    case CellMode::Qlc:
      return Result<void>::success();
  }
  return Result<void>::failure(
      {ErrorCode::kInvalidArgument, "unknown NAND cell mode"});
}

Result<TimingSelection> NandTimingModel::stages_for(
    MediaOp op, const MediaAddress& address, PageClass page_class,
    Temperature temperature) const {
  if (!std::isfinite(temperature)) {
    return Result<TimingSelection>::failure(
        {ErrorCode::kInvalidArgument, "temperature must be finite"});
  }
  // Erase is block-scoped and has no cell-page class. Select its mode table
  // explicitly so a TLC erase with the command default remains well-defined.
  PageClass selected_class = page_class;
  if (mode_ == CellMode::Tlc && page_class == PageClass::Default &&
      op != MediaOp::Erase) {
    // Synthetic layout: each wordline exposes LSB, CSB and MSB pages in that
    // order. Vendor layouts can replace this policy without leaking it upward.
    constexpr PageClass kTlcLayout[] = {
        PageClass::Lsb, PageClass::Csb, PageClass::Msb};
    selected_class = kTlcLayout[address.page % 3U];
  } else if (op == MediaOp::Erase) {
    selected_class = mode_ == CellMode::Tlc ? PageClass::Lsb
                                            : PageClass::Default;
  }
  auto raw = select_timing(mode_, selected_class);
  if (!raw) {
    return Result<TimingSelection>::failure(raw.error());
  }
  auto bank = bank_array_resource(address);
  auto channel_path = channel_media_path_resource(address);
  if (!bank) return Result<TimingSelection>::failure(bank.error());
  if (!channel_path) return Result<TimingSelection>::failure(channel_path.error());

  TimingSelection result;
  auto append = [&](StageKind kind, std::uint64_t nanoseconds,
                    std::vector<ResourceId> resources) -> Result<void> {
    auto duration = to_cycles(nanoseconds, cycle_ns_);
    if (!duration) return Result<void>::failure(duration.error());
    result.stages.push_back({kind, duration.value(), std::move(resources)});
    return Result<void>::success();
  };
  const RawTiming& t = *raw.value();
  Result<void> status = Result<void>::success();
  switch (op) {
    case MediaOp::Read:
      status = append(StageKind::ReadSense, t.read_sense, {bank.value()});
      if (status) status = append(StageKind::ReadDataOut, t.read_out,
                                  {channel_path.value()});
      break;
    case MediaOp::Program:
      status = append(StageKind::ProgramDataIn, t.program_in,
                      {channel_path.value()});
      if (status) status = append(StageKind::ProgramArray, t.program_array,
                                  {bank.value()});
      if (status) status = append(StageKind::ProgramVerify, t.program_verify,
                                  {bank.value()});
      break;
    case MediaOp::Erase:
      status = append(StageKind::EraseArray, t.erase_array, {bank.value()});
      if (status) status = append(StageKind::EraseVerify, t.erase_verify,
                                  {bank.value()});
      break;
    default:
      return Result<TimingSelection>::failure(
          {ErrorCode::kInvalidArgument, "unknown media operation"});
  }
  if (!status) return Result<TimingSelection>::failure(status.error());
  return Result<TimingSelection>::success(std::move(result));
}

const char* NandTimingModel::provenance() const noexcept {
  return "OpenHBF synthetic NAND timing defaults; not specified by OCP HBF";
}

}  // namespace openhbf::media
