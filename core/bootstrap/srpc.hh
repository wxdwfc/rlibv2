#pragma once

#include <utility> // std::pair

#include "./channel.hh"
#include "./multi_msg.hh"

namespace rdmaio {

namespace bootstrap {

/*!
  A simple RPC used for establish connection for RDMA.
 */
class SRpc {
  Arc<SendChannel> channel;

public:
  using rpc_id_t = u8;
  using MMsg = MultiMsg<kMaxMsgSz>;
  SRpc(const std::string &addr) : channel(SendChannel::create(addr).value()) {}

  /*!
    Send an RPC with id "id", using a specificed parameter.
    */
  Result<std::string> call(const rpc_id_t &id, const ByteBuffer &parameter) {
    if (sizeof(rpc_id_t) + parameter.size() > kMaxMsgSz) {
      return Err(std::string("Msg too large!, only kMaxMsgSz supported"));
    }
    MMsg mmsg(sizeof(rpc_id_t) + parameter.size());
    RDMA_ASSERT(mmsg.append(::rdmaio::Marshal::dump<rpc_id_t>(id)));
    RDMA_ASSERT(mmsg.append(parameter));
    return channel->send(mmsg.buf);
  }

  /*!
    Recv a reply from the server ysing the timeout specified.
    \Note: this call must follow from a "call"
    */
  Result<ByteBuffer> receive_reply(const double &timeout_usec = 1000) {
    auto res = channel->recv(timeout_usec);
    return res;
  }
};

class SRPcHandler {
  Arc<RecvChannel> channel;
};

} // namespace bootstrap

} // namespace rdmaio
