#pragma once

#include "../common.hh"
#include "../naming.hh"
#include "../nic.hh"

namespace rdmaio {

namespace qp {

const usize kMaxInlinSz = 64;

class Dummy {
public:
  struct ibv_qp *qp = nullptr;
  struct ibv_cq *cq = nullptr;
  struct ibv_cq *recv_cq = nullptr;

  ~Dummy() {
    // some clean ups
    if(qp) {
      int rc = ibv_destroy_qp(qp);
      RDMA_VERIFY(WARNING, rc == 0)
          << "Failed to destroy QP " << strerror(errno);
    }
  }

  Arc<RNic> nic;

  explicit Dummy(Arc<RNic> nic) : nic(nic) {}

  bool valid() const { return qp != nullptr && cq != nullptr; }

  // below handy helper functions for common QP operations
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

using ProgressMark_t = u16;
// Track the out-going and acknowledged reqs
struct Progress {
  static constexpr const u32 num_progress_bits = sizeof(ProgressMark_t) * 8;

  ProgressMark_t high_watermark = 0;
  ProgressMark_t low_watermark = 0;

  ProgressMark_t forward(ProgressMark_t num) {
    high_watermark += num;
    return high_watermark;
  }

  void done(int num) { low_watermark = num; }

  ProgressMark_t pending_reqs() const {
    if (high_watermark >= low_watermark)
      return high_watermark - low_watermark;
    return std::numeric_limits<ProgressMark_t>::max() -
           (low_watermark - high_watermark) + 1;
  }
};

} // namespace qp

} // namespace rdmaio

#include "rc.hh"
//#include "ud.hh"
