#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace openhbf {

// Broad error categories shared by module boundaries.  Module-specific status
// values remain in their owning module; Error is for validation/integrity APIs.
enum class ErrorCode {
  kInvalidArgument,
  kOutOfRange,
  kOverflow,
  kUnderflow,
  kDivisionByZero,
  kIntegrity,
  kUnsupported,
  kInternal,
};

struct Error {
  ErrorCode code = ErrorCode::kInternal;
  std::string message;

  friend bool operator==(const Error& lhs, const Error& rhs) {
    return lhs.code == rhs.code && lhs.message == rhs.message;
  }
  friend bool operator!=(const Error& lhs, const Error& rhs) {
    return !(lhs == rhs);
  }
};

// A small C++17 result type.  Callers must inspect the result before accessing
// value(); an invalid access is a programming error and throws logic_error.
template <typename T>
class Result {
 public:
  static Result success(T value) { return Result(std::move(value)); }
  static Result failure(Error error) { return Result(std::move(error)); }

  bool has_value() const noexcept { return std::holds_alternative<T>(data_); }
  explicit operator bool() const noexcept { return has_value(); }

  T& value() & {
    ensure_value();
    return std::get<T>(data_);
  }
  const T& value() const& {
    ensure_value();
    return std::get<T>(data_);
  }
  T&& value() && {
    ensure_value();
    return std::get<T>(std::move(data_));
  }

  Error& error() & {
    ensure_error();
    return std::get<Error>(data_);
  }
  const Error& error() const& {
    ensure_error();
    return std::get<Error>(data_);
  }

 private:
  explicit Result(T value) : data_(std::move(value)) {}
  explicit Result(Error error) : data_(std::move(error)) {}

  void ensure_value() const {
    if (!has_value()) {
      throw std::logic_error("attempted to access the value of a failed Result");
    }
  }
  void ensure_error() const {
    if (has_value()) {
      throw std::logic_error("attempted to access the error of a successful Result");
    }
  }

  std::variant<T, Error> data_;
};

template <>
class Result<void> {
 public:
  static Result success() { return Result(); }
  static Result failure(Error error) { return Result(std::move(error)); }

  bool has_value() const noexcept { return !has_error_; }
  explicit operator bool() const noexcept { return has_value(); }

  const Error& error() const {
    if (!has_error_) {
      throw std::logic_error("attempted to access the error of a successful Result");
    }
    return error_;
  }

 private:
  Result() = default;
  explicit Result(Error error) : has_error_(true), error_(std::move(error)) {}

  bool has_error_ = false;
  Error error_;
};

}  // namespace openhbf
