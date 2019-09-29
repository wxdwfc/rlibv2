#pragma once

#include "./mod.hh"

namespace rdmaio {

namespace qp {

class UD : public Dummy {
  /*!
    A local MR to to allow send
   */
  Option<RegAttr> local_mr;

  const QPConfig my_config;

public:
  UD(Arc<RNic> nic, const QPConfig &config) : Dummy(nic), my_config(config) {
    auto res = Impl::create_cq(nic, my_config.max_send_sz());

    if (res != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating CQ: " << std::get<1>(res.desc);
      return;
    }
    this->cq = std::get<0>(res.desc);

    auto res_recv = Impl::create_cq(nic,my_config.max_recv_sz());
    if (res_recv != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating recv CQ: " << std::get<1>(res.desc);
      return;
    }
    this->recv_cq = std::get<0>(res.desc);

    auto res_qp =
        Impl::create_qp(nic, IBV_QPT_UD, my_config, this->cq, this->recv_cq);
    if (res_qp != IOCode::Ok) {
      RDMA_LOG(4) << "Error on creating UD QP: " << std::get<1>(res.desc);
      return;
    }
    this->qp = std::get<0>(res_qp.desc);

    // finally, change it to ready_to_recv & ready_to_send
    // TODO;
  }
};
} // namespace qp

} // namespace rdmaio
