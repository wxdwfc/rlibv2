/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
#include <infiniband/verbs.h>
#include <tuple>

#include "./utils/logging.hh"
#include "./utils/option.hh"

namespace rdmaio {

struct __attribute__((packed)) IOCode {
  enum Code {
    Ok = 0,
    Err = 1,
    Timeout = 2,
    NearOk = 3, // there is some error, but may be tolerated
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
    default:
      assert(false); // should not happen
    }
  }

  inline bool operator==(const Code &code) { return c == code; }
};

struct __attribute__((packed)) DummyDesc {
  DummyDesc() = default;
};

/*!
  The abstract result of IO request, with a code and detailed descrption if
  error happens.
*/
template <typename Desc = DummyDesc> struct __attribute__((packed)) Result {
  IOCode code;
  Desc desc;
};

/*!
  Some wrapper functions for help creating results
  Example:
    auto res = Ok();
    assert(std::get<0)>(res) == IOCode::Ok);
 */
template <typename D = DummyDesc> inline Result<D> Ok(const D &d = D()) {
  return {.code = IOCode(IOCode::Ok), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> NearOk(const D &d = D()) {
  return {.code = IOCode(IOCode::NearOk), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> Err(const D &d = D()) {
  return {.code = IOCode(IOCode::Err), .desc = d};
}

template <typename D = DummyDesc> inline Result<D> Timeout(const D &d = D()) {
  return {.code = IOCode(IOCode::Timeout), .desc = d};
}

// some handy integer defines
using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using i64 = int64_t;
using u8 = uint8_t;
using i8 = int8_t;
using usize = unsigned int;

} // namespace rdmaio
