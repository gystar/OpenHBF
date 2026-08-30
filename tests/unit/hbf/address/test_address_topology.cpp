#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

#include "openhbx/hbf/address/block_sequence.h"
#include "openhbx/hbf/address/channel_topology.h"
#include "openhbx/hbf/address/hbf_address_mapper.h"
#include "openhbx/config/resolved_hbf_config.h"

using namespace openhbx;
using namespace openhbx::hbf::address;

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #value "\n"; ++failures; \
} } while (false)

HbfGeometry geometry() {
  // Non-symmetric R1/R2 makes transposition and accidental hard-coded modulo visible.
  auto result = HbfGeometry::create({2, 2, 1, 6, 5, 2, 3, 4, 64, 6});
  CHECK(result.has_value());
  return *result;
}

ChannelTopology product_topology(const HbfGeometry& geometry) {
  ChannelOwnershipProfile profile;
  profile.source = OwnershipSource::ProductProfile;
  profile.banks_by_channel = {
      {{CoreDieIndex(1), DieIndex(0), BankIndex(5)},
       {CoreDieIndex(0), DieIndex(0), BankIndex(1)},
       {CoreDieIndex(1), DieIndex(0), BankIndex(3)},
       {CoreDieIndex(0), DieIndex(0), BankIndex(4)},
       {CoreDieIndex(1), DieIndex(0), BankIndex(0)},
       {CoreDieIndex(0), DieIndex(0), BankIndex(2)}},
      {{CoreDieIndex(0), DieIndex(0), BankIndex(0)},
       {CoreDieIndex(1), DieIndex(0), BankIndex(1)},
       {CoreDieIndex(0), DieIndex(0), BankIndex(3)},
       {CoreDieIndex(1), DieIndex(0), BankIndex(2)},
       {CoreDieIndex(0), DieIndex(0), BankIndex(5)},
       {CoreDieIndex(1), DieIndex(0), BankIndex(4)}}};
  auto result = ChannelTopology::create(geometry, std::move(profile));
  CHECK(result.has_value());
  return *result;
}

void test_units_formula_and_round_trip() {
  const auto g = geometry();
  const auto topology = product_topology(g);
  HbfAddressMapper mapper(g, topology);
  CHECK(!mapper.map(ChannelId(0), LocalByteAddress(1)));

  // block=2, page=3, owned bank=4, sector=17.
  const std::uint64_t l1 = 2 * 6 * 4 + 3 * 6 + 4;
  const std::uint64_t bytes = (l1 * 64 + 17) * 64;
  const auto mapped = mapper.map(ChannelId(0), LocalByteAddress(bytes));
  CHECK(mapped);
  if (mapped) {
    CHECK(mapped.mapped->a1.value() == l1 * 64 + 17);
    CHECK(mapped.mapped->l1 == l1);
    CHECK(mapped.mapped->b1 == 2);
    CHECK(mapped.mapped->p1 == 22);
    CHECK(mapped.mapped->bank_num == 4);
    CHECK(mapped.mapped->l2 == 52);
    CHECK(mapped.mapped->address.page == PageIndex(3));
    CHECK(mapped.mapped->address.sector == SectorIndex(17));
    const auto reverse = mapper.reverse(mapped.mapped->address);
    CHECK(reverse && reverse.address->value() == bytes);
  }
  for (std::uint64_t channel = 0; channel < g.channels(); ++channel) {
    for (std::uint64_t value = 0; value < g.local_capacity_bytes(); value += 64) {
      const auto forward = mapper.map(ChannelId(channel), LocalByteAddress(value));
      CHECK(forward);
      if (forward) {
        const auto reverse = mapper.reverse(forward.mapped->address);
        CHECK(reverse && reverse.address->value() == value);
      }
    }
  }
  CHECK(!mapper.map(ChannelId(0), LocalByteAddress(g.local_capacity_bytes())));
}

void test_ownership_and_geometry_rejection() {
  const auto g = geometry();
  const auto topology = product_topology(g);
  CHECK(topology.source() == OwnershipSource::ProductProfile);
  const PhysicalBank bank{CoreDieIndex(1), DieIndex(0), BankIndex(5)};
  CHECK(topology.can_access(ChannelId(0), bank));
  CHECK(!topology.can_access(ChannelId(1), bank));
  HbfAddressMapper mapper(g, topology);
  HbfAddress wrong{ChannelId(1), bank, OwnedBankIndex(0), BlockIndex(0),
                   PageIndex(0), SectorIndex(0)};
  CHECK(!mapper.reverse(wrong));

  std::string error;
  auto overflow = HbfGeometry::create(
      {2, 2, 1, 6, std::numeric_limits<std::uint64_t>::max(), 2, 3, 4, 64, 6},
      &error);
  CHECK(!overflow && !error.empty());
  auto wrong_r5 = HbfGeometry::create({2, 2, 1, 6, 5, 2, 3, 4, 64, 7}, &error);
  CHECK(!wrong_r5);

  ChannelOwnershipProfile duplicate;
  duplicate.banks_by_channel.resize(2);
  duplicate.banks_by_channel[0].assign(6, bank);
  duplicate.banks_by_channel[1].assign(6, bank);
  CHECK(!ChannelTopology::create(g, std::move(duplicate)));
}

void test_synthetic_is_explicit() {
  auto g = HbfGeometry::create({2, 1, 1, 4, 2, 2, 1, 2, 64, 2});
  CHECK(g);
  auto profile = ChannelOwnershipProfile::synthetic_modulo(*g);
  CHECK(profile && profile->source == OwnershipSource::SyntheticFixture);
  auto topology = ChannelTopology::create(*g, std::move(*profile));
  CHECK(topology && topology->owner_of({CoreDieIndex(0), DieIndex(0), BankIndex(3)}) ==
                        ChannelId(1));
}

void test_sequence_order_and_stale() {
  const auto g = geometry();
  BlockSequence sequence(g);
  const BlockKey key{ChannelId(0), OwnedBankIndex(2), BlockIndex(3)};
  CHECK(!sequence.reserve_program(key, PageIndex(1), Token(1)));
  auto first = sequence.reserve_program(key, PageIndex(0), Token(2));
  CHECK(first && first.auto_erase_required);
  CHECK(sequence.reserve_program(key, PageIndex(0), Token(3)).error == SequenceError::Busy);
  CHECK(sequence.cancel_program(*first.reservation));
  CHECK(sequence.complete_program(*first.reservation, ProgramResult::Success).error ==
        SequenceError::Stale);
  first = sequence.reserve_program(key, PageIndex(0), Token(4));
  CHECK(sequence.complete_program(*first.reservation, ProgramResult::Success));
  CHECK(sequence.snapshot(key).expected_page == 1);
  CHECK(!sequence.reserve_program(key, PageIndex(2), Token(5)));
  const auto second = sequence.reserve_program(key, PageIndex(1), Token(6));
  CHECK(second && !second.auto_erase_required);
  CHECK(sequence.complete_program(*second.reservation, ProgramResult::Success));
  sequence.reset();
  CHECK(sequence.complete_program(*second.reservation, ProgramResult::Success).error ==
        SequenceError::Stale);
}

void test_legacy_ftl_keys_are_rejected() {
  openhbx::config::RawConfigTree raw;
  raw.scalars = {{"product.family", "HBF"},
                 {"product.profile", "OCP_HBF_0_7"},
                 {"features.gc", "true"}};
  const auto result = openhbx::config::resolve_hbf_config(raw);
  CHECK(!result);
  CHECK(!result.issues().empty());
  const auto issue = std::find_if(result.issues().begin(), result.issues().end(),
                                  [](const openhbx::config::ConfigIssue& candidate) {
    return candidate.code == openhbx::config::ConfigErrorCode::UnknownKey &&
           candidate.path == "features.gc";
  });
  CHECK(issue != result.issues().end());
}
}  // namespace

int main() {
  test_units_formula_and_round_trip();
  test_ownership_and_geometry_rejection();
  test_synthetic_is_explicit();
  test_sequence_order_and_stale();
  test_legacy_ftl_keys_are_rejected();
  if (failures != 0) std::cerr << failures << " address unit checks failed\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
