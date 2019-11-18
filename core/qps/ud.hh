#pragma once

#include "./mod.hh"

namespace rdmaio {

namespace qp {

/*!
  an abstraction of unreliable datagram
  example usage:
  `
  // TODO
  `
 */
class UD : public Dummy {
  /*!
    a msg should fill in one packet (4096 bytes); some bytes are reserved
    for header (GRH size)
  */
  const usize kMaxMsgSz = 4000;

  usize pending_reqs = 0;

public:
  const QPConfig my_config;

  static Option<Arc<UD>> create(Arc<RNic> nic, const QPConfig &config) {
    auto ud_ptr = Arc<UD>(new UD(nic, config));
    if (ud_ptr->valid())
      return ud_ptr;
    return {};
  }

  inline usize get_pending_reqs() const { return pending_reqs; }

  Result<std::string> send() { return Err(std::string("not implemented")); }

private:
  UD(Arc<RNic> nic, const QPConfig &config) : Dummy(nic), my_config(config) {

    // create qp, cq, recv_cq
    auto res = Impl::create_cq(nic, my_config.max_send_sz());

    if (res != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating CQ: " << std::get<1>(res.desc);
      return;
    }
    this->cq = std::get<0>(res.desc);

    auto res_recv = Impl::create_cq(nic, my_config.max_recv_sz());
    if (res_recv != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating recv CQ: " << std::get<1>(res.desc);
      return;
    }
    this->recv_cq = std::get<0>(res.desc);

    // this is a trick: if recv qp is null, than qp must be null
    auto res_qp =
        Impl::create_qp(nic, IBV_QPT_UD, my_config, this->cq, this->recv_cq);
    if (res_qp != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating UD QP: " << std::get<1>(res.desc);
      return;
    }
    this->qp = std::get<0>(res_qp.desc);

    // finally, change it to ready_to_recv & ready_to_send
    if (valid()) {

      // 1. change to ready to init
      auto res_init =
          Impl::bring_qp_to_init(this->qp, this->my_config, this->nic);
      if (res_init != IOCode::Ok) {
        // shall we assert false? this is rarely unlikely the case
        RDMA_ASSERT(false)
            << "bring ud to init error, not handlered failure case: "
            << res_init.desc;
      } else {
      }

      // 2. change to ready_to_recv
      if (!bring_ud_to_recv(this->qp) ||
          !bring_ud_to_send(this->qp, this->my_config.rq_psn)) {
        this->cq = nullptr; // make my status invalid
      }
    }
    // done, UD create done
  }

  //
  static bool bring_ud_to_recv(ibv_qp *qp) {
    int rc, flags = IBV_QP_STATE;
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTR;

    rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }

  static bool bring_ud_to_send(ibv_qp *qp, int psn) {
    int rc, flags = 0;
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.sq_psn = psn;

    flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
    rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }

};
} // namespace qp

} // namespace rdmaio
