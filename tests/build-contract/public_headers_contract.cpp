#include "openhbx/config/build_contract.h"
#include "openhbx/common/build_contract.h"
#include "openhbx/hbf/address/build_contract.h"
#include "openhbx/hbf/controller/build_contract.h"
#include "openhbx/hbf/host/build_contract.h"
#include "openhbx/integration/build_contract.h"
#include "openhbx/interconnect/build_contract.h"
#include "openhbx/media/nand/build_contract.h"
#include "openhbx/pal/build_contract.h"
#include "openhbx/ras/build_contract.h"
#include "openhbx/system/build_contract.h"

#include <array>
#include <string_view>

int main() {
  const std::array<std::string_view, 11> domains = {
      openhbx::config::build_contract_domain(),
      openhbx::common::build_contract_domain(),
      openhbx::system::build_contract_domain(),
      openhbx::integration::build_contract_domain(),
      openhbx::hbf::host::build_contract_domain(),
      openhbx::hbf::address::build_contract_domain(),
      openhbx::hbf::controller::build_contract_domain(),
      openhbx::interconnect::build_contract_domain(),
      openhbx::pal::build_contract_domain(),
      openhbx::media::nand::build_contract_domain(),
      openhbx::ras::build_contract_domain(),
  };
  for (const auto domain : domains) {
    if (domain.empty()) {
      return 1;
    }
  }
  return 0;
}
