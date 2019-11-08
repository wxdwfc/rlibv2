#pragma once

#include "./bootstrap/proto.hh"
#include "./bootstrap/srpc.hh"

#include "./rmem/factory.hh"

namespace rdmaio {

using namespace bootstrap;

/*!
  A connect manager tailed for Making RDMA establishments
  The functions including:
  - Fetch remote MR
  - Create and Fetch a QP for connect

  RCtrl (defined in rctrl.hh) provides the end-point support for calling these
  functions at the server. User can also register their specified callbacks, see
  rctrl.hh for detailes.

  Example usage (fetch a remote registered MR, identified by the id)
  \note: we must first make sure that server must **started** and the handler
  must be **registered**. Otherwise, all calls will timeout

  `
  ConnectManager cm("localhost:1111");
  if(cm.wait_ready(1000) == IOCode::Timeout) // wait 1 second for server to ready
    assert(false);

  rmem::RegAttr attr;
  auto mr_res = cm.fetch_remote_mr(1,attr); // fetch "localhost:1111"'s MR with id
  **1**
  if (mr_res == IOCode::Ok) {
  // use attr ...
  }
  `
 */
class ConnectManager {
  SRpc rpc;

public:
  explicit ConnectManager(const std::string &addr) : rpc(addr) {}

  Result<std::string> wait_ready(const double &timeout_usec,
                                 const usize &retry = 1) {
    for (uint i = 0; i < retry; ++i) {
      // send a dummy request to the RPC
      auto res = rpc.call(proto::RCtrlBinderIdType::HeartBeat, ByteBuffer(1, '0'));
      if (res != IOCode::Ok && res != IOCode::Timeout)
        // some error occurs, abort
        return res;
      auto res_reply = rpc.receive_reply(timeout_usec,true); // receive as hearbeat message
      if (res_reply == IOCode::Ok)
        return res_reply;
    }
    return Timeout(std::string("retry exceeds num"));
  }

  /*!
    Fetch remote MR identified with "id" at remote machine of this connection
    manager, store the result in the "attr".
   */
  Result<std::string> fetch_remote_mr(const rmem::register_id_t &id,
                                      rmem::RegAttr &attr,
                                      const double &timeout_usec = 1000000) {
    auto res = rpc.call(proto::FetchMr,
                        ::rdmaio::Marshal::dump<proto::MRReq>({.id = id}));
    auto res_reply = rpc.receive_reply(timeout_usec);
    if (res_reply == IOCode::Ok) {
      // further we check the status by decoding the reply
      try {
        auto mr_reply =
            ::rdmaio::Marshal::dedump<proto::MRReply>(res_reply.desc).value();
        switch (mr_reply.status) {
        case proto::CallbackStatus::Ok:
          attr = mr_reply.attr;
          return ::rdmaio::Ok(std::string(""));
        case proto::CallbackStatus::NotFound:
          return NotReady(std::string(""));
        default:
          return ::rdmaio::Err(std::string("Error return status code"));
        }

      } catch (std::exception &e) {
        return ::rdmaio::Err(std::string("Decode reply"));
      }
    }
    return res_reply;
  }
};

} // namespace rdmaio
