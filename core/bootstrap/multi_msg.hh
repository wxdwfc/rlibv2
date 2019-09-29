#pragma once

#include <limits>

#include "../common.hh"
#include "./channel.hh"

#include "../utils/marshal.hh"

namespace rdmaio {

namespace bootstrap {

// max encoded msg per MultBuffer
const usize kMaxMultiMsg = 8;

struct __attribute__((packed)) MultiEntry {
  u8 sz = 0;
  u16 offset = 0;

  static usize max_entry_sz() { return std::numeric_limits<u8>::max(); }
};

struct __attribute__((packed)) MultiHeader {
  u8 num = 0;
  MultiEntry entries[kMaxMultiMsg];
};

/*!
  MultiBuffer encodes several ByteBuffer into one single one,
  and we can decode these buffers separately.
  Note that at most *kMaxMultiMsg* can be encoded.
 */
struct MultBuffer {
  MultiHeader *header;
  ByteBuffer buf;

  MultBuffer() {
    buf.reserve(kMaxMsgSz);

    MultiHeader header;
    buf.append(::rdmaio::Marshal::dump<MultiHeader>(header));

    { // unsafe code
      this->header = (MultiHeader *)(buf.data());
    }
  }
};

} // namespace bootstrap
} // namespace rdmaio
