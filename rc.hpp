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

struct QPAttr {
  QPAttr(const qp_address_t &addr,int lid,int qpn,int psn,int port_id):
      addr(addr),lid(lid),qpn(qpn),psn(psn),port_id(port_id){
  }
  QPAttr() {}
  qp_address_t addr;
  int lid;
  int qpn;
  int psn;
  int port_id;
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
      port_id(rnic.id.port_id),
      attr(rnic.addr,rnic.lid,0,config.rq_psn,rnic.id.port_id) {
    cq_ = create_cq(rnic, config.max_send_size);
    if(two_sided)
      recv_cq_ = create_cq(rnic,config.max_recv_size);
    else
      recv_cq_ = cq_;
    qp_ = create_qp(rnic, config, cq_, recv_cq_);
    if(qp_ != nullptr) attr.qpn = qp_->qp_num;
  }

  IOStatus connect(const QPAttr &attr, const RCConfig &config) {
    if(qp_status() == IBV_QPS_RTS) return SUCC;
    if(!bring_qp_to_rcv(qp_, config, attr, port_id)) return ERR;
    if(!bring_qp_to_send(qp_, config)) return ERR;
    return SUCC;
  }

  struct ReqMeta {
    ibv_wr_opcode op;
    int flags;
    uint32_t len   = 0;
    uint64_t wr_id = 0;
  };

  struct ReqContent {
    char *local_buf = nullptr;
    uint64_t remote_addr = 0;
    uint64_t imm_data    = 0;
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

  IOStatus poll_completion(ibv_wc &wc,const Duration_t &timeout = no_timeout) {
    return poll_completion_helper(cq_,wc,timeout);
  }

 private:
  RemoteMemory::Attr remote_mem_;
  RemoteMemory::Attr local_mem_;
  QPAttr    attr;

 public:
  QPAttr    get_attr() const { return attr;}
  const     int    lid;
  const     int    port_id;
  Progress  progress_;

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

  static bool bring_qp_to_rcv(ibv_qp *qp,const RCConfig &config,const QPAttr &attr,int port_id) {
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state              = IBV_QPS_RTR;
    qp_attr.path_mtu              = IBV_MTU_4096;
    qp_attr.dest_qp_num           = attr.qpn;
    qp_attr.rq_psn                = config.rq_psn; // should this match the sender's psn ?
    qp_attr.max_dest_rd_atomic    = config.max_dest_rd_atomic;
    qp_attr.min_rnr_timer         = 20;

    qp_attr.ah_attr.dlid          = attr.lid;
    qp_attr.ah_attr.sl            = 0;
    qp_attr.ah_attr.src_path_bits = 0;
    qp_attr.ah_attr.port_num      = port_id; /* Local port id! */

    qp_attr.ah_attr.is_global                     = 1;
    qp_attr.ah_attr.grh.dgid.global.subnet_prefix = attr.addr.subnet_prefix;
    qp_attr.ah_attr.grh.dgid.global.interface_id  = attr.addr.interface_id;
    qp_attr.ah_attr.grh.sgid_index                = 0;
    qp_attr.ah_attr.grh.flow_label                = 0;
    qp_attr.ah_attr.grh.hop_limit                 = 255;

    int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN
                | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    auto rc = ibv_modify_qp(qp, &qp_attr,flags);
    return rc == 0;
  }

  static bool bring_qp_to_send(ibv_qp *qp,const RCConfig &config) {
    int rc, flags;
    struct ibv_qp_attr qp_attr = {};

    qp_attr.qp_state           = IBV_QPS_RTS;
    qp_attr.sq_psn             = config.sq_psn;
    qp_attr.timeout            = config.timeout;
    qp_attr.retry_cnt          = 7;
    qp_attr.rnr_retry          = 7;
    qp_attr.max_rd_atomic      = config.max_rd_atomic;
    qp_attr.max_dest_rd_atomic = config.max_dest_rd_atomic;

    flags = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
            IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &qp_attr,flags);
    return rc == 0;
  }

 private:
  /**
   * return whether qp is in {INIT,READ_TO_RECV,READY_TO_SEND} states
   */
  ibv_qp_state qp_status() const {
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    RDMA_ASSERT(ibv_query_qp(qp_, &attr,IBV_QP_STATE, &init_attr) == 0);
    return attr.qp_state;
  }
}; // end class QP

} // end namespace rdmaio
