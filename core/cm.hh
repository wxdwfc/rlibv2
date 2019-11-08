#pragma once

#include "./bootstrap/srpc.hh"
#include "./bootstrap/proto.hh"

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
  if(cm.wait_ready(1000) == IOCode::Timeout) // wait 1 second for server to
  ready assert(false);

  auto mr_res = cm.fetch_remote_mr(1); // fetch "localhost:1111"'s MR with id **1**
  if (mr_res == IOCode::Ok) {
    // operation on mr_res.desc (defined in rmem/factory.hh, **RegAttr**)
  }
  `
 */
class ConnectManager {
  SRpc rpc;

  Result<std::string> wait_ready(const double &timeout_usec) {
    // send a dummy request to the RPC
    auto res = rpc.call(proto::Heartbeat, ByteBuffer(1,'0'));
    if (res != IOCode::Ok)
      return res;
    auto res_reply = rpc.receive_reply(timeout_usec);
  }
};


} // namespace rdmaio
