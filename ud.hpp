#pragma once

#include "common.hpp"
#include "util.hpp"
#include "memory.hpp"

namespace rdmaio {

class UDQP : public QPDummy {
 public:
  UDQP(RNic &rnic,
       const RemoteMemory::Attr &local_mem,
       const QPConfig &config) :
      local_mem_(local_mem),
      rnic_p(&rnic),
      max_send_size(config.max_send_size),
      attr(rnic.addr,rnic.lid,config.rq_psn,rnic.id.port_id) {

    // create cq for send messages
    cq_ = QPUtily::create_cq(rnic, config.max_send_size);
    if(cq_ == nullptr)
      return;

    // create recv queue for receiving messages
    recv_cq_ = QPUtily::create_cq(rnic,config.max_recv_size);
    if(recv_cq_ == nullptr)
      return;

    qp_ = QPUtily::create_qp(IBV_QPT_UD,rnic, config, cq_, recv_cq_);

    /**
     * bring qp to the valid state: ready to recv + ready to send
     */
    if(qp_) {
      if(!bring_ud_to_recv(qp_) || !bring_ud_to_send(qp_,config.rq_psn)) {
        delete qp_;
        qp_ = nullptr;
      } else {
        // re-fill entries
        attr.qpn  = qp_->qp_num;
        attr.qkey = config.qkey;
      }
    }
  }

  /**
   * Some methods to help manage requests progress of this UD QP
   */
  inline int  num_pendings() const { return pending_reqs_; }
  bool empty() const { return num_pendings() == 0;  }

  inline bool need_poll() const {
    return num_pendings() >= (max_send_size / 2);
  }

  inline void forward(int num) { pending_reqs_ += 1; }
  inline void clear() { pending_reqs_ = 0; }

  QPAttr    get_attr() const { return attr;}

 public:
  const RemoteMemory::Attr local_mem_;
  const RNic              *rnic_p;
  const int                max_send_size;
 private:
  int                      pending_reqs_ = 0;
  QPAttr                   attr;

  static bool bring_ud_to_recv(ibv_qp *qp) {
	int rc, flags = IBV_QP_STATE;
	struct ibv_qp_attr qp_attr = {};
	qp_attr.qp_state = IBV_QPS_RTR;

	rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }

  static bool bring_ud_to_send(ibv_qp *qp,int psn) {
	int rc, flags = 0;
	struct ibv_qp_attr qp_attr = {};
	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.sq_psn   = psn;

	flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
	rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }
}; // end class UDQP

}  // end namespace
