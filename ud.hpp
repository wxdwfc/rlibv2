#pragma once

#include "common.hpp"
#include "util.hpp"

namespace rdmaio {

class UDQP : public QPDummy {
 public:
  UDQP(RNic &rnic,
       const RemoteMemory::Attr &local_mem,
       const QPConfig &config) :
      local_mem_(local_mem),
      attr(rnic.addr,rnic.lid,config.rq_psn,rnic.id.port_id) {
    cq_ = QPUtily::create_cq(rnic, config.max_send_size);
    if(cq_ == nullptr)
      return;
    recv_cq_ = QPUtily::create_cq(rnic,config.max_recv_size);
    if(recv_cq_ == nullptr)
      return;
    qp_ = QPUtily::create_qp(rnic, config, cq_, recv_cq_);
    if(!bring_ud_to_recv(qp_)) {
      delete qp_;
      qp_ = nullptr;
    }
  }

  int num_pendings() const { return pending_reqs_; }

  QPAttr    get_attr() const { return attr;}

 private:
  const RemoteMemory::Attr local_mem_;
  int                      pending_reqs_ = 0;
  QPAttr                   attr;

  static bool bring_ud_to_recv(ibv_qp *qp) {
	int rc, flags = IBV_QP_STATE;
	struct ibv_qp_attr qp_attr = {};
	qp_attr.qp_state = IBV_QPS_RTR;

	rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }
}; // end class UDQP

}  // end namespace
