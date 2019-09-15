#pragma once

#include "../common.hh"
#include "../naming.hh"

namespace rdmaio {

namespace qp {

class Dummy {
public:
  struct ibv_qp *qp = nullptr;
  struct ibv_cq *cq = nullptr;
  struct ibv_cq *recv_cq = nullptr;

  bool valid() const { return qp != nullptr && cq != nullptr; }

  /**
   * return whether qp is in {INIT,READ_TO_RECV,READY_TO_SEND} states
   */
  Result<ibv_qp_state> qp_status() const {
    if (!valid()) {
      return Err(IBV_QPS_RTS); // note it is just a dummy value, because there
                               // is already an error
    }
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    RDMA_ASSERT(ibv_query_qp(qp, &attr, IBV_QP_STATE, &init_attr) == 0);
    return Ok(attr.qp_state);
  }
};

/*!
  Below structures are make packed to allow communicating
  between servers
 */
struct __attribute__((packed)) QPAttr {
  RAddress addr;
  u64 lid;
  u64 psn;
  u64 port_id;
  u64 qpn;
  u64 qkey;
};

using progress_type = u16;
// Track the out-going and acknowledged reqs
struct Progress {
  static constexpr const u32 num_progress_bits = sizeof(progress_type) * 8;

  progress_type high_watermark = 0;
  progress_type low_watermark = 0;

  progress_type forward(progress_type num) {
    high_watermark += num;
    return high_watermark;
  }

  void done(int num) { low_watermark = num; }

  progress_type pending_reqs() const {
    if (high_watermark >= low_watermark)
      return high_watermark - low_watermark;
    return std::numeric_limits<progress_type>::max() -
           (low_watermark - high_watermark) + 1;
  }
};

} // namespace qp

} // namespace rdmaio

#include "rc.hh"
//#include "ud.hh"
