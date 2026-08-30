#include "openhbf/media/persistence.h"

#include <cstddef>
#include <limits>
#include <string>

namespace openhbf::media {
namespace {

constexpr std::uint32_t kMagic = 0x4d464248U;  // "HBFM" in little-endian.
constexpr std::uint16_t kSchemaVersion = 2;

void put_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
  out.push_back(value);
}
void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  for (unsigned shift = 0; shift < 16; shift += 8)
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
  std::uint32_t crc = 0xffffffffU;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

class Reader {
 public:
  explicit Reader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
  Result<std::uint8_t> u8() { return take<std::uint8_t>(1); }
  Result<std::uint16_t> u16() { return take<std::uint16_t>(2); }
  Result<std::uint32_t> u32() { return take<std::uint32_t>(4); }
  Result<std::uint64_t> u64() { return take<std::uint64_t>(8); }
  std::size_t position() const noexcept { return position_; }
  bool at_end() const noexcept { return position_ == bytes_.size(); }

 private:
  template <typename T>
  Result<T> take(std::size_t count) {
    if (count > bytes_.size() - position_) {
      return Result<T>::failure(
          {ErrorCode::kIntegrity, "truncated media metadata image"});
    }
    T value = 0;
    for (std::size_t i = 0; i < count; ++i)
      value |= static_cast<T>(bytes_[position_++]) << (i * 8U);
    return Result<T>::success(value);
  }
  const std::vector<std::uint8_t>& bytes_;
  std::size_t position_ = 0;
};

void put_geometry(std::vector<std::uint8_t>& out, const Geometry& geometry) {
  put_u16(out, geometry.channels);
  put_u16(out, geometry.core_dies_per_channel);
  put_u16(out, geometry.dies_per_core);
  put_u16(out, geometry.banks_per_die);
  put_u32(out, geometry.blocks_per_bank);
  put_u32(out, geometry.pages_per_block);
}

Result<Geometry> read_geometry(Reader& reader) {
  auto channels = reader.u16(); auto core = reader.u16();
  auto dies = reader.u16(); auto banks = reader.u16();
  auto blocks = reader.u32(); auto pages = reader.u32();
  if (!channels || !core || !dies || !banks || !blocks || !pages)
    return Result<Geometry>::failure(
        {ErrorCode::kIntegrity, "truncated metadata geometry"});
  return Result<Geometry>::success({channels.value(), core.value(), dies.value(),
                                    banks.value(), blocks.value(), pages.value()});
}

bool same_geometry(const Geometry& a, const Geometry& b) noexcept {
  return a.channels == b.channels &&
      a.core_dies_per_channel == b.core_dies_per_channel &&
      a.dies_per_core == b.dies_per_core &&
      a.banks_per_die == b.banks_per_die &&
      a.blocks_per_bank == b.blocks_per_bank &&
      a.pages_per_block == b.pages_per_block;
}

void put_address(std::vector<std::uint8_t>& out, const MediaAddress& address) {
  put_u16(out, address.channel.value()); put_u16(out, address.core_die);
  put_u16(out, address.die); put_u16(out, address.bank);
  put_u32(out, address.block);
}

Result<MediaAddress> read_address(Reader& reader) {
  auto channel = reader.u16(); auto core = reader.u16();
  auto die = reader.u16(); auto bank = reader.u16(); auto block = reader.u32();
  if (!channel || !core || !die || !bank || !block)
    return Result<MediaAddress>::failure(
        {ErrorCode::kIntegrity, "truncated metadata address"});
  return Result<MediaAddress>::success(
      {ChannelId(channel.value()), core.value(), die.value(), bank.value(),
       block.value(), 0});
}

}  // namespace

Result<std::vector<std::uint8_t>> encode_metadata(
    const MediaMetadataImage& image) {
  auto valid = validate(image.geometry);
  if (!valid) return Result<std::vector<std::uint8_t>>::failure(valid.error());
  if (image.blocks.size() > std::numeric_limits<std::uint32_t>::max() ||
      image.bbt.entries.size() > std::numeric_limits<std::uint32_t>::max() ||
      image.retirement.entries.size() > std::numeric_limits<std::uint32_t>::max()) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::kOverflow, "metadata entry count exceeds schema"});
  }
  std::vector<std::uint8_t> payload;
  put_u32(payload, static_cast<std::uint32_t>(image.blocks.size()));
  for (const auto& block : image.blocks) {
    put_u64(payload, block.key); put_u32(payload, block.state.expected_page);
    put_u64(payload, block.state.program_erase_count);
    put_u64(payload, block.state.epoch); put_u64(payload, block.state.read_count);
    put_u64(payload, block.state.last_program_cycle.value());
  }
  put_u64(payload, image.bbt.version);
  put_u32(payload, static_cast<std::uint32_t>(image.bbt.entries.size()));
  for (const auto& entry : image.bbt.entries) {
    put_address(payload, entry.block_address);
    put_u8(payload, static_cast<std::uint8_t>(entry.reason));
    put_u64(payload, entry.marked_cycle.value()); put_u64(payload, entry.pe_cycles);
  }
  put_u64(payload, image.retirement.version);
  put_u32(payload, static_cast<std::uint32_t>(image.retirement.entries.size()));
  for (const auto& entry : image.retirement.entries) {
    put_address(payload, entry.block_address);
    put_u8(payload, static_cast<std::uint8_t>(entry.reason));
    put_u64(payload, entry.retired_cycle.value());
  }
  if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
    return Result<std::vector<std::uint8_t>>::failure(
        {ErrorCode::kOverflow, "metadata payload exceeds schema"});
  }
  std::vector<std::uint8_t> output;
  put_u32(output, kMagic); put_u16(output, kSchemaVersion); put_u16(output, 0);
  put_geometry(output, image.geometry);
  put_u32(output, static_cast<std::uint32_t>(payload.size()));
  put_u32(output, crc32(payload.data(), payload.size()));
  output.insert(output.end(), payload.begin(), payload.end());
  return Result<std::vector<std::uint8_t>>::success(std::move(output));
}

Result<MediaMetadataImage> decode_metadata(
    const std::vector<std::uint8_t>& bytes, const Geometry& expected_geometry) {
  Reader reader(bytes);
  auto magic = reader.u32(); auto version = reader.u16(); auto reserved = reader.u16();
  auto geometry = read_geometry(reader); auto length = reader.u32(); auto checksum = reader.u32();
  if (!magic || !version || !reserved || !geometry || !length || !checksum ||
      magic.value() != kMagic || version.value() != kSchemaVersion ||
      reserved.value() != 0 || !same_geometry(geometry.value(), expected_geometry) ||
      length.value() != bytes.size() - reader.position() ||
      crc32(bytes.data() + reader.position(), length.value()) != checksum.value()) {
    return Result<MediaMetadataImage>::failure(
        {ErrorCode::kIntegrity, "invalid metadata header, geometry, length or checksum"});
  }
  MediaMetadataImage image;
  image.geometry = geometry.value();
  auto block_count = reader.u32();
  if (!block_count) return Result<MediaMetadataImage>::failure(block_count.error());
  for (std::uint32_t i = 0; i < block_count.value(); ++i) {
    auto key = reader.u64(); auto expected = reader.u32(); auto pec = reader.u64();
    auto epoch = reader.u64(); auto reads = reader.u64(); auto programmed = reader.u64();
    if (!key || !expected || !pec || !epoch || !reads || !programmed)
      return Result<MediaMetadataImage>::failure(
          {ErrorCode::kIntegrity, "truncated block metadata"});
    image.blocks.push_back({key.value(), {expected.value(), pec.value(),
        epoch.value(), reads.value(), Cycle(programmed.value())}});
  }
  auto bbt_version = reader.u64(); auto bad_count = reader.u32();
  if (!bbt_version || !bad_count)
    return Result<MediaMetadataImage>::failure({ErrorCode::kIntegrity, "truncated BBT header"});
  image.bbt.version = bbt_version.value();
  for (std::uint32_t i = 0; i < bad_count.value(); ++i) {
    auto address = read_address(reader); auto reason = reader.u8();
    auto cycle = reader.u64(); auto pec = reader.u64();
    if (!address || !reason || !cycle || !pec || reason.value() > static_cast<std::uint8_t>(BadReason::Wear))
      return Result<MediaMetadataImage>::failure({ErrorCode::kIntegrity, "invalid BBT entry"});
    image.bbt.entries.push_back({address.value(), static_cast<BadReason>(reason.value()),
                                 Cycle(cycle.value()), pec.value()});
  }
  auto retire_version = reader.u64(); auto retire_count = reader.u32();
  if (!retire_version || !retire_count)
    return Result<MediaMetadataImage>::failure({ErrorCode::kIntegrity, "truncated retirement header"});
  image.retirement.version = retire_version.value();
  for (std::uint32_t i = 0; i < retire_count.value(); ++i) {
    auto address = read_address(reader); auto reason = reader.u8(); auto cycle = reader.u64();
    if (!address || !reason || !cycle || reason.value() > static_cast<std::uint8_t>(RetireReason::Administrative))
      return Result<MediaMetadataImage>::failure({ErrorCode::kIntegrity, "invalid retirement entry"});
    image.retirement.entries.push_back({address.value(),
        static_cast<RetireReason>(reason.value()), Cycle(cycle.value())});
  }
  if (!reader.at_end())
    return Result<MediaMetadataImage>::failure({ErrorCode::kIntegrity, "metadata image has trailing bytes"});
  return Result<MediaMetadataImage>::success(std::move(image));
}

}  // namespace openhbf::media
