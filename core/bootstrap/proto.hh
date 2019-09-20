#pragma once

#include "../common.hh"
#include "../utils/marshal.hh"

namespace rdmaio {

/*!
  Meta data passed between bootstrap procedures
  Supported max reply/receive size is limited for this SimpleRPC
 */
using reply_size_t = u16;
using req_size_t = u16;

struct __attribute__((packed)) ReqHeader {
  using elem_t = struct __attribute__((packed)) {
    u16 type;
    u16 payload;
  };
  static const u8 max_batch_sz = 4;
  elem_t elems[max_batch_sz];
  u8 total_reqs = 0;
};

struct __attribute__((packed)) ReplyHeader {
  reply_size_t reply_sizes[ReqHeader::max_batch_sz];
  u8 total_replies = 0;
};

/*!
  a ReqResc = (req_type, req)
*/
using ReqDesc = std::pair<u8, ByteBuffer>;
} // namespace rdmaio
