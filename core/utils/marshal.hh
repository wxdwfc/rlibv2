#pragma once

#include <string>

#include "../common.hh"

namespace rdmaio {

using U8buf = std::string;

/*!
  A simple, basic use of helper methods for marshaling/unmarshaling data from
  byte buffer.
  A bytebuffer is basically a c++ string. Any alternative containiner is also
  suitable.

  Example:
  1. get a u8 buf initialized with 0.
    ` auto buf = Marshal::alloc_buf(size);`
  2. Dump a struct to a buf, and dedump it to get it.
  ` struct __attribute__((packed)) A { // note struct must be packed
      u8 a;
      u64 b;
      };
    A temp;
    U8buf buf = Marshal::dump(temp);
    assert(Marshal::dedump(buf).value() == temp); // note this is an option.
    `
 */
class Marshal {
public:
  static U8buf alloc_buf(usize size) { return U8buf(size, '0'); }

  /*!
    Subtract the current buffer at offset.
    If offset is smaller than the buf.size(), then return an U8buf.
    Otherwise, return {}.
   */
  static Option<U8buf> forward(const U8buf &buf, usize off) {
    if (off < buf.size()) {
      return Option<U8buf>(buf.substr(off, buf.size() - off));
    }
    return {};
  }

  static bool safe_set_byte(U8buf &buf, usize off, u8 data) {
    if (off < buf.size()) {
      *((u8 *)(buf.data() + off)) = data; // unsafe code
      return true;
    }
    return false;
  }

  template <typename T> static U8buf dump(const T &t) {
    auto buf = alloc_buf(sizeof(T));
    memcpy((void *)buf.data(), &t, sizeof(T)); // unsafe code
    return buf;
  }

  template <typename T> static Option<T> dedump(const U8buf &b) {
    if (b.size() >= sizeof(T)) {
      T res;
      memcpy((char *)(&res), b.data(), sizeof(T));
      return Option<T>(res);
    }
    return {};
  }
};

} // namespace rdmaio
