#pragma once

#include "./multi_msg.hh"

namespace rdmaio {

namespace bootstrap {

/*!
  MsgsIter provides a means to help iterating through the batched msg.
  Return a pointer and the size of the Msg.

  Example usage of an unsafe version (but without string copy):
  `
  MultiMsg<1024> msgs;
  for (MsgsIter iter(msgs); iter.valid();iter.next()) {
    auto msg = iter.cur();
    auto msg_ptr = std::get<0>(msg);
    auto msg_sz  = std::get<1>(msg);
  }
  `

  Example usage of a safe version:
  `
  MultiMsg<1024> msgs;
  for (MsgsIter iter(msgs); iter.valid();iter.next()) {
    auto msg = iter.cur_msg();
    // To use ...
  }
  `
 */
class MsgsIter {
  const MultiMsg *msgs_p;
  usize cur_idx = 0;

public:
  explicit MsgsIter(const MultiMsg &msgs) : msgs_p(&msgs) {}

  bool valid() const {
    return cur_idx < msgs_p->num_msg();
  }

  void next() {
    cur_idx += 1;
  }

  std::pair<char *, usize> cur() const {
    RDMA_ASSERT(valid());
    MsgEntry &entry = msgs_p->header->entries[cur_idx];
    return
  }
};
}
} // namespace rdmaio
