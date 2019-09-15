#pragma once

#include "../rmem/handler.hh"
#include "./config.hh"
#include "./mod.hh"

namespace rdmaio {

using namespace rmem;

namespace qp {

/*!
  To use:
  Arc<RC> rc = ...;
  class Class_use_rc {
  Arc<RC> rc;

  Class_use_rc(Arc<RC> rr) : rc(std::move(rr) {
  // std::move here is to avoid copy from shared_ptr
  }
  };

  Class_use_rc cur(rc);
  // to use cur
  }
*/
class RC : public Dummy {
  // default local MR used by this QP
  Option<RegAttr> local_mr;
  // default remote MR used by this QP
  Option<RegAttr> remote_mr;

  // connect info used by others
  QPAttr my_attr;

  // pending requests monitor
  Progress progress;

 public:
  const QPConfig my_config;

  explicit RC(const QPConfig &config = QPConfig()) : my_config(config) {
    RDMA_LOG(2) << "rc init called";
  }

  /*!
    bind a mr to remote/local mr(so that QP is able to access its memory)
   */
  void bind_remote_mr(const RegAttr &mr) { remote_mr = Option<RegAttr>(mr); }

  void bind_local_mr(const RegAttr &mr) { local_mr = Option<RegAttr>(mr); }

  /*!
    connect this QP to a remote qp attr
    \note: this connection is setup locally.
    The `attr` must be fetched from another machine.
   */
  Result<std::string> connect(const QPAttr &attr) {
    auto res = qp_status();
    if (res.code == IOCode::Ok) {
      auto status = res.desc;
      switch (status) {
      case IBV_QPS_RTS:
        return Ok(std::string("already connected"));
        // TODO: maybe we should check other status
      default:
        // really connect the QP
        {
          // first bring QP to ready to recv. note we bring it to ready to init
          // during class's construction.
          auto res = bring_rc_to_rcv(qp, my_config, attr, my_attr.port_id);
          if (res.code != IOCode::Ok)
            return res;
          // then we bring it to ready to send.
          res = bring_rc_to_send(qp, my_config);
          if (res.code != IOCode::Ok)
            return res;
        }
        return Ok(std::string(""));
      }
    } else {
      return Err(std::string("QP not valid"));
    }
  }

  /*!
    Post a send request to the QP.
    the request can be:
    - ibv_wr_send (send verbs)
    - ibv_wr_read (one-sided read)
    - ibv_wr_write (one-sided write)
   */
  struct ReqDesc {
    ibv_wr_opcode op;
    int flags;
    u32 len = 0;
    u64 wr_id = 0;
  };

  struct ReqPayload {
    rmem::RMem::raw_ptr_t local_addr = nullptr;
    u64 remote_addr = 0;
    u64 imm_data = 0;
  };

  /*!
    the version of send_normal without passing a MR.
    it will use the default MRs binded to this QP.
    \note: this function will panic if no MR is bind to this QP.
   */
  Result<std::string> send_normal(const ReqDesc &desc,
                                  const ReqPayload &payload) {
    return send_normal(desc, payload, local_mr.value(), remote_mr.value());
  }

  Result<std::string> send_normal(const ReqDesc &desc,
                                  const ReqPayload &payload,
                                  const RegAttr &local_mr,
                                  const RegAttr &remote_mr) {
    struct ibv_sge sge {
      .addr = (u64)(payload.local_addr), .length = desc.len,
      .lkey = local_mr.key
    };

    struct ibv_send_wr sr, *bad_sr;

    sr.wr_id = (static_cast<u64>(desc.wr_id) << Progress::num_progress_bits) |
               static_cast<u64>(progress.forward(1));
    sr.opcode = desc.op;
    sr.num_sge = 1;
    sr.next = nullptr;
    sr.sg_list = &sge;
    sr.send_flags = desc.flags;
    sr.imm_data = payload.imm_data;

    sr.wr.rdma.remote_addr = remote_mr.buf + payload.remote_addr;
    sr.wr.rdma.rkey = remote_mr.key;

    auto rc = ibv_post_send(qp, &sr, &bad_sr);
    if (0 == rc) {
      return Ok(std::string(""));
    }
    return Err(std::string(strerror(errno)));
  }

  int max_send_sz() const { return my_config.max_send_size; }

private:
  // helper functions
  static Result<std::string> bring_rc_to_rcv(ibv_qp *qp, const QPConfig &config,
                                             const QPAttr &attr, int port_id) {
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTR;
    qp_attr.path_mtu = IBV_MTU_4096;
    qp_attr.dest_qp_num = attr.qpn;
    qp_attr.rq_psn = config.rq_psn; // should this match the sender's psn ?
    qp_attr.max_dest_rd_atomic = config.max_dest_rd_atomic;
    qp_attr.min_rnr_timer = 20;

    qp_attr.ah_attr.dlid = attr.lid;
    qp_attr.ah_attr.sl = 0;
    qp_attr.ah_attr.src_path_bits = 0;
    qp_attr.ah_attr.port_num = port_id; /* Local port id! */

    qp_attr.ah_attr.is_global = 1;
    qp_attr.ah_attr.grh.dgid.global.subnet_prefix = attr.addr.subnet_prefix;
    qp_attr.ah_attr.grh.dgid.global.interface_id = attr.addr.interface_id;
    qp_attr.ah_attr.grh.sgid_index = 0;
    qp_attr.ah_attr.grh.flow_label = 0;
    qp_attr.ah_attr.grh.hop_limit = 255;

    int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER;
    auto rc = ibv_modify_qp(qp, &qp_attr, flags);
    if (rc != 0)
      return Err(std::string(strerror(errno)));
    return Ok(std::string(""));
  }

  static Result<std::string> bring_rc_to_send(ibv_qp *qp,
                                              const QPConfig &config) {
    int rc, flags;
    struct ibv_qp_attr qp_attr = {};

    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.sq_psn = config.sq_psn;
    qp_attr.timeout = config.timeout;
    qp_attr.retry_cnt = 7;
    qp_attr.rnr_retry = 7;
    qp_attr.max_rd_atomic = config.max_rd_atomic;
    qp_attr.max_dest_rd_atomic = config.max_dest_rd_atomic;

    flags = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &qp_attr, flags);
    if (rc != 0)
      return Err(std::string(strerror(errno)));
    return Ok(std::string(""));
  }
};

} // namespace qp

} // namespace rdmaio
