#pragma once

#include <cstdint>
#include <vector>

#include "openhbf/media/bbt.h"
#include "openhbf/media/state.h"

namespace openhbf::media {

struct MediaMetadataImage {
  Geometry geometry;
  std::vector<BlockMetadata> blocks;
  BbtSnapshot bbt;
  RetirementSnapshot retirement;
};

// The encoding is little-endian and independent of C++ struct layout. Header:
// magic, schema version, geometry, payload length and CRC32. decode validates
// the complete image before returning any state to the caller.
Result<std::vector<std::uint8_t>> encode_metadata(
    const MediaMetadataImage& image);
Result<MediaMetadataImage> decode_metadata(
    const std::vector<std::uint8_t>& bytes, const Geometry& expected_geometry);

}  // namespace openhbf::media
