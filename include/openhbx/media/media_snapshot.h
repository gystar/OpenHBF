#pragma once

#include <cstddef>
#include <cstdint>

namespace openhbx::media {
struct MediaSnapshot {
  std::size_t inflight{0};
  std::size_t queued_events{0};
  std::uint64_t committed_pages{0};
  std::uint64_t bad_blocks{0};
  std::uint64_t stale_events{0};
};
}  // namespace openhbx::media
