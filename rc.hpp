#pragma once

#include "common.hpp"
#include "qp_config.hpp"
#include "memory.hpp"

namespace rdmaio {

// Track the out-going and acknowledged reqs
struct Progress {
  uint64_t high_watermark = 0;
  uint64_t low_watermark  = 0;

  void forward(int num) {
    high_watermark += num;
  }

  void done(int num) {
    low_watermark += num;
  }

  uint64_t pending_reqs() const {
    return high_watermark - low_watermark;
  }
};

class RCQP : public QPDummy {
 public:
  explicit RCQP(RNic &rnic,
                const RemoteMemory::Attr &remote_mem,
                const RemoteMemory::Attr &local_mem,
                const RCConfig &config,bool two_sided = false) :
      remote_mem_(remote_mem),
      local_mem_(local_mem),
      lid(rnic.lid),
      addr(rnic.addr)
  {
    cq_ = create_cq(rnic, config.max_send_size);
    if(two_sided)
      recv_cq_ = create_cq(rnic,config.max_recv_size);
    qp_ = create_qp(rnic, config, cq_, recv_cq_);
  }

 private:
  RemoteMemory::Attr remote_mem_;
  RemoteMemory::Attr local_mem_;
  Progress           progress_;
 public:
  const     qp_address_t addr;
  const              int lid;

 public:
  static ibv_cq * create_cq(const RNic &rnic,int size) {
    return ibv_create_cq(rnic.ctx, size , nullptr, nullptr, 0);
  }

  static ibv_qp * create_qp(const RNic &rnic,const RCConfig &config,
                            ibv_cq *send_cq,ibv_cq *recv_cq) {
    struct ibv_qp_init_attr qp_init_attr = {};

    qp_init_attr.send_cq = send_cq;
    qp_init_attr.recv_cq = recv_cq;
    qp_init_attr.qp_type = IBV_QPT_RC;

    qp_init_attr.cap.max_send_wr = config.max_send_size;
    qp_init_attr.cap.max_recv_wr = config.max_recv_size;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = MAX_INLINE_SIZE;

    auto qp = ibv_create_qp(rnic.pd, &qp_init_attr);
    if(qp != nullptr) {
      auto res = bring_qp_to_init(qp, rnic, config);
      if(res) return qp;
    }
    return nullptr;
  }

  static bool bring_qp_to_init(ibv_qp *qp,const RNic &rnic,const RCConfig &config) {

    if(qp != nullptr) {
      struct ibv_qp_attr qp_attr = {};
      qp_attr.qp_state           = IBV_QPS_INIT;
      qp_attr.pkey_index         = 0;
      qp_attr.port_num           = rnic.id.port_id;
      qp_attr.qp_access_flags    = config.access_flags;

      int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
      int rc = ibv_modify_qp(qp, &qp_attr,flags);
      RDMA_VERIFY(WARNING,rc == 0) <<  "Failed to modify RC to INIT state, %s\n" <<  strerror(errno);

      if(rc != 0) {
        RDMA_LOG(WARNING) << " change state to init failed. ";
        return false;
      }
      return true;
    }
    return false;
  }
}; // end class QP

} // end namespace rdmaio
