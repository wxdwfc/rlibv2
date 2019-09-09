#pragma once

#include <string>

#include "../common.hh"

namespace rdmaio {

using U8buf = std::string;

class Marshal {
 public:
  static U8buf alloc_buf(usize size) {
    return U8buf(size,'0');
  }

  /*!
    Subtract the current buffer at offset.
    If offset is smaller than the buf.size(), then return an U8buf.
    Otherwise, return {}.
   */
  static Option<U8buf> forward(const U8buf &buf,usize off) {
    if(off < buf.size()) {
      return Option<U8buf>(buf.substr(off,buf.size() - off));
    }
    return {};
  }

  static bool safe_set_byte(U8buf &buf,usize off, u8 data) {
    if (off < buf.size()) {
      *((u8 *)(buf.data() + off)) = data; // unsafe code
      return true;
    }
    return false;
  }
};

}
