#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace openhbx {

class PayloadHandle {
 public:
  PayloadHandle() = default;
  static PayloadHandle from_bytes(std::vector<std::uint8_t> bytes) {
    return PayloadHandle(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)));
  }
  bool empty() const noexcept { return !bytes_; }
  std::size_t size() const noexcept { return bytes_ ? bytes_->size() : 0; }
  const std::vector<std::uint8_t>& bytes() const noexcept {
    static const std::vector<std::uint8_t> empty;
    return bytes_ ? *bytes_ : empty;
  }
  void reset() noexcept { bytes_.reset(); }
  bool shares_storage_with(const PayloadHandle& other) const noexcept {
    return bytes_ == other.bytes_;
  }
 private:
  explicit PayloadHandle(std::shared_ptr<const std::vector<std::uint8_t>> bytes)
      : bytes_(std::move(bytes)) {}
  std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
};

}  // namespace openhbx
