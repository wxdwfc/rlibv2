#pragma once

#include "./bootstrap/proto.hh"
#include "./bootstrap/srpc.hh"

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

  rmem::RegAttr attr;
  auto mr_res = cm.fetch_remote_mr(1,attr); // fetch "localhost:1111"'s MR with
  id
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
      auto res =
          rpc.call(proto::RCtrlBinderIdType::HeartBeat, ByteBuffer(1, '0'));
      if (res != IOCode::Ok && res != IOCode::Timeout)
        // some error occurs, abort
        return res;
      auto res_reply =
          rpc.receive_reply(timeout_usec, true); // receive as hearbeat message
      if (res_reply == IOCode::Ok)
        return res_reply;
    }
    return Timeout(std::string("retry exceeds num"));
  }

  Result<std::string>
  delete_remote_rc(const ::rdmaio::qp::register_id_t &id,
                   const u64 &key, const double &timeout_usec = 1000000) {
    auto res = rpc.call(
        proto::DeleteRC,
        ::rdmaio::Marshal::dump<proto::DelRCReq>({.id = id, .key = key}));
    auto res_reply = rpc.receive_reply(timeout_usec);
    if (res_reply == IOCode::Ok) {
      try {
        auto qp_reply =
            ::rdmaio::Marshal::dedump<RCReply>(res_reply.desc).value();
        switch (qp_reply.status) {
        case proto::CallbackStatus::Ok:
          return ::rdmaio::Ok(std::string(""));
        case proto::CallbackStatus::WrongArg:
          return ::rdmaio::Err(std::string("wrong arg"));
        case proto::CallbackStatus::AuthErr:
          return ::rdmaio::Err(std::string("auth failure"));
        }
      } catch (std::exception &e) {
      }
    }
    ::rdmaio::Err(std::string("fatal error"));
  }

  /*!
    *C*reate and *C*onnect an RC QP at remote end.
    This function first creates an RC QP at local,
    and then create an RC QP at remote host to connect with the local
    created QP.

    \param:
    - id: the remote naming of this QP
    - nic_id: the remote NIC used for connect the pair QP
   */
  Result<std::string> cc_rc(const ::rdmaio::qp::register_id_t &id,
                            const Arc<::rdmaio::qp::RC> rc, u64 &key,
                            const ::rdmaio::nic_id_t &nic_id,
                            const ::rdmaio::qp::QPConfig &config,
                            const double &timeout_usec = 1000000) {

    auto res = rpc.call(proto::CreateRC, ::rdmaio::Marshal::dump<proto::RCReq>(
                                             {.id = id,
                                              .whether_create = 1,
                                              .nic_id = nic_id,
                                              .config = config,
                                              .attr = rc->my_attr()}));
    auto res_reply = rpc.receive_reply(timeout_usec);

    Result<std::string> ret = ::rdmaio::Err(std::string(""));

    if (res_reply == IOCode::Ok) {
      try {
        auto qp_reply =
            ::rdmaio::Marshal::dedump<RCReply>(res_reply.desc).value();
        switch (qp_reply.status) {
        case proto::CallbackStatus::Ok: {
          ret = rc->connect(qp_reply.attr);
          if (ret != IOCode::Ok)
            goto ErrCase;
          key = qp_reply.key;
          return ret;
        }
        case proto::CallbackStatus::ConnectErr:
          ret = ::rdmaio::Err(std::string("Remote connect error"));
          goto ErrCase;
        case proto::CallbackStatus::WrongArg:
          ret = ::rdmaio::Err(std::string("Wrong arguments"));
          goto ErrCase;
        default:
          ret = ::rdmaio::Err(std::string("Error return status code"));
        }
      } catch (std::exception &e) {
        ret = ::rdmaio::Err(std::string("Decode reply"));
      }
    }

  ErrCase:
    return ret;
  }

  /*!
    Fetch remote MR identified with "id" at remote machine of this
    connection manager, store the result in the "attr".
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
