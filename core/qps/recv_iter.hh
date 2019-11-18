#pragma once

#include "./entities.h"

namespace rdmaio {

namespace qp {

/*!
  RecvIter helps traversing two-sided messages, using recv_cq.

  \note:  we donot store the smart pointer for the performance reason

  Example:
  `
  Arc<UD> ud; // we have an UD
  RecvEntries<12> entries;

  for(RecvIter iter(ud, entries); iter.has_msgs(); iter.next()) {
    auto imm_data, msg_buf = iter.cur_msg(); // relax some syntax
    // do with imm_data and msg_buf
  }
  `
 */
template <typename QP, usize entries> class RecvIter {
  QP &qp;
  RecvEntries<entries> &entries;

  int idx = 0;
  const int total_msgs = 0;

public:
  RecvIter(Arc<QP> &qp, RecvEntries<entries> &entries)
      : qp(*(qp.get())), entries(entries),
        total_msgs(ibv_poll_cq(qp.recv_cq, entries.wcs)) {}

  /*!
    \ret (imm_data, recv_buffer)
    */
  Option<::rmem::RMem::raw_ptr_t> cur_msg() const {
    if (has_msgs()) {
      auto buf = reinterpret_cast <::rmem::RMem::raw_ptr_t>(entries.wcs[idx].imm_data);
      return std::make_pair(entries.wcs[idx].imm_data,buf);
    }
    return {};
  }

  inline void next() {
    idx += 1;
  }

  inline bool has_msgs() const { return idx < total_msgs; }

  ~RecvIter() {
    // post recvs
    if (likely(total_msgs > 0)) {
      auto res = qp.post_recvs<entries>(entries,total_msgs);
      if (unlikely(res != IOCode::Ok))
        RDMA_LOG(4) << "post recv error: " << strerror(res.desc);
    }
  }
};

} // namespace qp
} // namespace rdmaio
