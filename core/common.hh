/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
#include <infiniband/verbs.h>
#include <tuple>
#include <memory>

#include "./utils/logging.hh"
#include "./utils/option.hh"

namespace rdmaio {

struct __attribute__((packed)) IOCode {
  enum Code {
    Ok = 0,
    Err = 1,
    Timeout = 2,
    NearOk = 3, // there is some error, but may be tolerated
    NotReady = 4,
  };

  Code c;

  explicit IOCode(const Code &c) : c(c) {}

  std::string name() {
    switch (c) {
    case Ok:
      return "Ok";
    case Err:
      return "Err";
    case Timeout:
      return "Timeout";
    case NearOk:
      return "NearOk";
    case NotReady:
      return "NotReady";
    default:
      RDMA_ASSERT(false); // should not happen
    }
    return "";
  }

  inline bool operator==(const Code &code) { return c == code; }

  inline bool operator!=(const Code &code) { return c != code; }
};

struct __attribute__((packed)) DummyDesc {
  DummyDesc() = default;
};

/*!
  The abstract result of IO request, with a code and detailed descrption if
  error happens.
*/
template <typename Desc = DummyDesc> struct Result {
  IOCode code;
  Desc desc;

  inline bool operator==(const IOCode::Code &c) { return code.c == c; }

  inline bool operator!=(const IOCode::Code &c) { return code.c != c; }
};

/*!
  Some wrapper functions for help creating results
  Example:
    auto res = Ok();
    assert(res == IOCode::Ok);
 */
template <typename D = DummyDesc> inline Result<D> Ok(const D &d = D()) {
  return {.code = IOCode(IOCode::Ok), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> NearOk(const D &d = D()) {
  return {.code = IOCode(IOCode::NearOk), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> Err(const D &d = D()) {
  return {.code = IOCode(IOCode::Err), .desc = std::move(d)};
}

template <typename D = DummyDesc> inline Result<D> Timeout(const D &d = D()) {
  return {.code = IOCode(IOCode::Timeout), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> NotReady(const D &d = D()) {
  return {.code = IOCode(IOCode::NotReady), .desc = d};
}

template <typename A, typename B>
inline Result<B> transfer(const Result<A> &a, const D &d) {
  return { .code = a.code, .desc = d };
}

// some handy integer defines
using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using i64 = int64_t;
using u8 = uint8_t;
using i8 = int8_t;
using usize = unsigned int;

// some handy alias for smart pointer
template <typename T>
using Arc = std::shared_ptr<T>;

#ifndef DISABLE_COPY_AND_ASSIGN
#define DISABLE_COPY_AND_ASSIGN(classname)                                     \
private:                                                                       \
  classname(const classname &) = delete;                                       \
  classname &operator=(const classname &) = delete
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

} // namespace rdmaio
