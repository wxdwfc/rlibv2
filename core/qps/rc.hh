#pragma once

#include "../rmem/handler.hh"

#include "./mod.hh"
#include "./impl.hh"

namespace rdmaio {

using namespace rmem;

namespace qp {

/*!
  To use:
  Arc<RC> rc = RC::create(...).value();
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

  // pending requests monitor
  Progress progress;

  const QPConfig my_config;

  /* the only constructor
     make it private because it may failed to create
     use the factory method to create it:
     Option<Arc<RC>> qp = RC::create(nic,config);
     if(qp) {
     ...
     }
  */
  RC(Arc<RNic> nic, const QPConfig &config)
      : Dummy(nic), my_config(config) {
    /*
      It takes 3 steps to create an RC QP during the initialization
      according to the RDMA programming mannal.
      First, we create the cq for completions of send request.
      Then, we create the qp.
      Finally, we change the qp to read_to_init status.
     */
    // 1 cq
    auto res = Impl::create_cq(nic,my_config.max_send_sz());
    if(res != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating CQ: " << std::get<1>(res.desc);
      return;
    }
    this->cq = std::get<0>(res.desc);

    // 2 qp
    auto res_qp = Impl::create_qp(nic,IBV_QPT_RC,my_config,this->cq,this->recv_cq);
    if(res_qp != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating QP: " << std::get<1>(res.desc);
      return;
    }
    this->qp = std::get<0>(res_qp.desc);

    // 3 -> init
    auto res_init = Impl::bring_qp_to_init(this->qp, this->my_config,this->nic);
    if(res_init != IOCode::Ok) {
      RDMA_LOG(4) << "failed to bring QP to init: " << res_init.desc;
    }
  }

 public:
  static Option<Arc<RC>> create(Arc<RNic> nic, const QPConfig &config = QPConfig()) {
    auto res = Arc<RC>(new RC(nic,config));
    if(res->valid()) {
      return Option<Arc<RC>>(std::move(res));
    }
    return {};
  }

  QPAttr my_attr() const {
    return {
      .addr = nic->addr.value(),
      .lid  = nic->lid.value(),
      .psn  = static_cast<u64>(my_config.rq_psn),
      .port_id = static_cast<u64>(nic->id.port_id),
      .qpn = static_cast<u64>(qp->qp_num),
      .qkey = static_cast<u64>(0)
    };
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
          auto res = Impl::bring_rc_to_rcv(qp, my_config, attr, my_attr().port_id);
          if (res.code != IOCode::Ok)
            return res;
          // then we bring it to ready to send.
          res = Impl::bring_rc_to_send(qp, my_config);
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
};

} // namespace qp

} // namespace rdmaio
