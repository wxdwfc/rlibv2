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
  RCQP(RNic &rnic,
       const RemoteMemory::Attr &remote_mem,
       const RemoteMemory::Attr &local_mem,
       const RCConfig &config,bool two_sided = false) :
      remote_mem_(remote_mem),
      local_mem_(local_mem),
      lid(rnic.lid),
      addr(rnic.addr) {
    cq_ = create_cq(rnic, config.max_send_size);
    if(two_sided)
      recv_cq_ = create_cq(rnic,config.max_recv_size);
    qp_ = create_qp(rnic, config, cq_, recv_cq_);
  }

  typedef struct {
    ibv_wr_opcode op;
    int flags;
    uint32_t len   = 0;
    uint64_t wr_id = 0;
  } ReqMeta;

  typedef struct ReqContent {
    char *local_buf = nullptr;
    uint64_t remote_addr;
    uint64_t imm_data;
  };

  IOStatus send(const ReqMeta &meta,const ReqContent &req) {
    return send(meta,req,remote_mem_,local_mem_);
  }

  IOStatus send(const ReqMeta &meta,const ReqContent &req,
                const RemoteMemory::Attr &remote_attr,
                const RemoteMemory::Attr &local_attr) {
    // setting the SGE
    struct ibv_sge sge {
      .addr = (uint64_t)(req.local_buf),
      .length = meta.len,
      .lkey   = local_attr.key
    };

    struct ibv_send_wr sr, *bad_sr;

    sr.wr_id        = meta.wr_id;
    sr.opcode       = meta.op;
    sr.num_sge      = 1;
    sr.next         = nullptr;
    sr.sg_list      = &sge;
    sr.send_flags   = meta.flags;
    sr.imm_data     = req.imm_data;

    sr.wr.rdma.remote_addr = remote_attr.buf + req.remote_addr;
    sr.wr.rdma.rkey        = remote_attr.key;

    auto rc = ibv_post_send(qp_,&sr,&bad_sr);
    return rc == 0 ? SUCC : ERR;
  }

  int poll_send_cq(ibv_wc &wc) {
    return ibv_poll_cq(cq_,1,&wc);
  }

 private:
  RemoteMemory::Attr remote_mem_;
  RemoteMemory::Attr local_mem_;
 public:
  const     qp_address_t addr;
  const              int lid;
  Progress               progress_;

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
