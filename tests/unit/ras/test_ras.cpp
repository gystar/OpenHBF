#include <cstdlib>
#include "openhbx/ras/bad_block_table.h"
#include "openhbx/ras/reliability_model.h"

int main() {
  openhbx::ras::BadBlockTable bbt;
  if (!bbt.mark(7, openhbx::ras::BadBlockReason::Factory) ||
      bbt.mark(7, openhbx::ras::BadBlockReason::Retired) || !bbt.is_bad(7) ||
      bbt.version() != 1) return EXIT_FAILURE;
  openhbx::ras::ReliabilityModel a({123, 2}), b({123, 2});
  if (a.evaluate_read(8, 0, 25.0) != b.evaluate_read(8, 0, 25.0)) return EXIT_FAILURE;
  if (a.evaluate_read(8, 0, 25.0) != openhbx::ras::RawReadClass::Refresh) return EXIT_FAILURE;
  a.force_failure(4, openhbx::ras::ForcedFailure::Program);
  if (!a.fails(4, openhbx::ras::ForcedFailure::Program)) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
