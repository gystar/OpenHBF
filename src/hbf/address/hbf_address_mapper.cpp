#include "openhbx/hbf/address/hbf_address_mapper.h"

#include "openhbx/common/checked_math.h"

namespace openhbx::hbf::address {
namespace {
MapResult map_error(AddressError error, const char* detail) {
  return {std::nullopt, error, detail};
}
}  // namespace

const char* to_string(AddressError error) noexcept {
  switch (error) {
    case AddressError::None: return "None";
    case AddressError::Misaligned: return "Misaligned";
    case AddressError::OutOfRange: return "OutOfRange";
    case AddressError::Overflow: return "Overflow";
    case AddressError::WrongOwner: return "WrongOwner";
    case AddressError::NonCanonical: return "NonCanonical";
    case AddressError::Retired: return "Retired";
    case AddressError::InvalidGeometry: return "InvalidGeometry";
  }
  return "Unknown";
}

MapResult HbfAddressMapper::map(ChannelId channel, LocalByteAddress address) const {
  if (channel.value() >= geometry_.channels()) return map_error(AddressError::WrongOwner, "channel");
  if (address.value() % 64 != 0) return map_error(AddressError::Misaligned, "64-byte A1 unit");
  if (address.value() >= geometry_.local_capacity_bytes()) {
    return map_error(AddressError::OutOfRange, "channel-local capacity");
  }
  const std::uint64_t a1 = address.value() / 64;
  const std::uint64_t l1 = a1 / geometry_.r4();
  const auto bank_page_span = checked_mul(geometry_.r1(), geometry_.r2());
  const auto block_span = bank_page_span ? checked_mul(*bank_page_span, geometry_.r3())
                                         : std::nullopt;
  if (!block_span) return map_error(AddressError::Overflow, "R1*R2*R3");
  const std::uint64_t b1 = l1 / *block_span;
  const std::uint64_t p1 = l1 % *block_span;
  const std::uint64_t bank_num = p1 % *bank_page_span;
  const std::uint64_t page = p1 / *bank_page_span;
  if (b1 >= geometry_.blocks_per_bank() || page >= geometry_.r3()) {
    return map_error(AddressError::OutOfRange, "block/page");
  }
  const auto bank = topology_.physical_bank(channel, OwnedBankIndex(bank_num));
  if (!bank) return map_error(AddressError::WrongOwner, "owned bank");
  BlockIndex physical_block(b1);
  AddressViewEpoch epoch(0);
  if (zones_ != nullptr) {
    const auto lookup = zones_->lookup(ZoneId(b1));
    if (!lookup) return map_error(AddressError::Retired, "zone");
    physical_block = lookup->physical_block;
    epoch = lookup->epoch;
  }
  const auto block_base = checked_mul(b1, *block_span);
  const auto l2 = block_base ? checked_add(*block_base, bank_num) : std::nullopt;
  if (!l2) return map_error(AddressError::Overflow, "L2");
  MappedAddress mapped{{channel, *bank, OwnedBankIndex(bank_num), physical_block,
                        PageIndex(page), SectorIndex(a1 % geometry_.r4())},
                       epoch, A1Units64(a1), l1, b1, p1, bank_num, *l2};
  return {mapped, AddressError::None, {}};
}

ReverseResult HbfAddressMapper::reverse(const HbfAddress& address) const {
  if (address.channel.value() >= geometry_.channels() ||
      address.page.value() >= geometry_.r3() ||
      address.sector.value() >= geometry_.r4() ||
      !topology_.can_access(address.channel, address.physical_bank)) {
    return {std::nullopt, AddressError::WrongOwner};
  }
  const auto owned = topology_.owned_index(address.channel, address.physical_bank);
  if (!owned || *owned != address.owned_bank) {
    return {std::nullopt, AddressError::NonCanonical};
  }
  BlockIndex logical_block = address.block;
  if (zones_ != nullptr) {
    const auto logical = zones_->reverse(address.block);
    if (!logical) return {std::nullopt, AddressError::Retired};
    logical_block = BlockIndex(logical->value());
  }
  if (logical_block.value() >= geometry_.blocks_per_bank()) {
    return {std::nullopt, AddressError::OutOfRange};
  }
  const auto bank_span = checked_mul(geometry_.r1(), geometry_.r2());
  auto l1 = bank_span ? checked_mul(logical_block.value(), geometry_.r3()) : std::nullopt;
  if (l1) l1 = checked_mul(*l1, *bank_span);
  const auto page_offset = bank_span ? checked_mul(address.page.value(), *bank_span)
                                     : std::nullopt;
  if (l1 && page_offset) l1 = checked_add(*l1, *page_offset);
  if (l1) l1 = checked_add(*l1, address.owned_bank.value());
  auto a1 = l1 ? checked_mul(*l1, geometry_.r4()) : std::nullopt;
  if (a1) a1 = checked_add(*a1, address.sector.value());
  const auto bytes = a1 ? checked_mul(*a1, 64) : std::nullopt;
  if (!bytes || *bytes >= geometry_.local_capacity_bytes()) {
    return {std::nullopt, AddressError::Overflow};
  }
  return {LocalByteAddress(*bytes), AddressError::None};
}

}  // namespace openhbx::hbf::address
