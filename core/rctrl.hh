#pragma once

#include "./qps/factory.hh"
#include "./rmem/factory.hh"

#include "./bootstrap/proto.hh"

#include <atomic>

namespace rdmaio {

/*!
  RCtrl is a control path daemon, that handles all RDMA bootstrap to this
  machine. RCtrl is **thread-safe**.
 */
class RCtrl {

  std::atomic<bool> running;
  std::mutex lock;

  pthread_t handler_tid_;

  /*!
    The two factory which allow user to **register** the QP, MR so that others
    can establish communication with them.
   */
  rmem::RegFactory registered_mrs;
  qp::Factory registeted_qps;

  bootstrap::SRpcHandler rpc;

public:
  explicit RCtrl(const usize &port) : running(false), rpc(port) {
    RDMA_ASSERT(rpc.register_handler(
        proto::Heartbeat,
        std::bind(&RCtrl::heartbeat_handler, this, std::placeholders::_1)));

    RDMA_ASSERT(rpc.register_handler(
        proto::FetchMr,
        std::bind(&RCtrl::fetch_mr_handler, this, std::placeholders::_1)));
  }

  // handlers of the dameon call
private:
  ByteBuffer heartbeat_handler(const ByteBuffer &b) {
    return ByteBuffer(""); // a null reply is ok
  }

  ByteBuffer fetch_mr_handler(const ByteBuffer &b) {
    auto o_id = ::rdmaio::Marshal::dedump<proto::MRReq>(b);
    if (o_id) {
      auto req_id = o_id.value();
      auto o_attr = registered_mrs.get_attr_byid(req_id.id);
      if (o_attr) {
        return ::rdmaio::Marshal::dump<proto::MRReply>(
            {.status = proto::CallbackStatus::Ok, .attr = o_attr.value()});

      } else {
        return ::rdmaio::Marshal::dump<proto::MRReply>(
            {.status = proto::CallbackStatus::NotFound});
      }
    }
    return ::rdmaio::Marshal::dump<proto::MRReply>(
        {.status = proto::CallbackStatus::WrongArg});
  }
};

} // namespace rdmaio
