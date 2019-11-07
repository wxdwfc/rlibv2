#pragma once

#include <mutex>   // lock
#include <utility> // std::pair

#include "./channel.hh"
#include "./multi_msg.hh"

namespace rdmaio {

namespace bootstrap {

using rpc_id_t = u8;

enum CallStatus {
  Ok = 0,
  Nop,
  WrongReply,
};

/*!
  A simple RPC used for establish connection for RDMA.
 */
class SRpc {
  Arc<SendChannel> channel;

public:
  using MMsg = MultiMsg<kMaxMsgSz>;
  SRpc(const std::string &addr) : channel(SendChannel::create(addr).value()) {}

  /*!
    Send an RPC with id "id", using a specificed parameter.
    */
  Result<std::string> call(const rpc_id_t &id, const ByteBuffer &parameter) {
    auto mmsg_o = MMsg::create_exact(sizeof(rpc_id_t) + parameter.size());
    if (mmsg_o) {
      auto &mmsg = mmsg_o.value();
      RDMA_ASSERT(mmsg.append(::rdmaio::Marshal::dump<rpc_id_t>(id)));
      RDMA_ASSERT(mmsg.append(parameter));
      return channel->send(mmsg.buf);
    } else {
      return Err(std::string("Msg too large!, only kMaxMsgSz supported"));
    }
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

class SRpcHandler;
class RPCFactory {
  friend class SRpcHandler;
  /*!
  A simple RPC function:
  handle(const ByteBuffer &req) -> ByteBuffer
   */
  using req_handler_f = std::function<ByteBuffer(const ByteBuffer &req)>;
  std::map<rpc_id_t, req_handler_f> registered_handlers;

  std::mutex lock;

public:
  bool register_handler(rpc_id_t id, req_handler_f val) {
    std::lock_guard<std::mutex> guard(lock);
    if (registered_handlers.find(id) == registered_handlers.end()) {
      registered_handlers.insert(std::make_pair(id, val));
      return true;
    }
    return false;
  }

  ByteBuffer call_one(rpc_id_t id, const ByteBuffer &parameter) {
    auto fn = registered_handlers.find(id)->second;
    return fn(parameter);
  }
};

class SRpcHandler {
  Arc<RecvChannel> channel;
  RPCFactory factory;

public:
  explicit SRpcHandler(const usize &port)
      : channel(RecvChannel::create(port).value()) {}

  bool register_handler(rpc_id_t id, RPCFactory::req_handler_f val) {
    return factory.register_handler(id, val);
  }

  /*!
    Run a event loop to call received RPC calls
    \ret: number of PRCs served
   */
  usize run_one_event_loop() {
    usize count = 0;
    for (channel->start(); channel->has_msg(); channel->next(), count += 1) {
      auto &msg = channel->cur();

      try {
        // create from move the cur_msg to a MuiltiMsg
        auto segmeneted_msg = MultiMsg<kMaxMsgSz>::create_from(msg).value();

        // query the RPC call id
        rpc_id_t id = ::rdmaio::Marshal::dedump<rpc_id_t>(
                                                          segmeneted_msg.query_one(0).value()).value();
        ByteBuffer parameter = segmeneted_msg.query_one(1).value();

        // call the RPC
        ByteBuffer reply = factory.call_one(id, parameter);

        MultiMsg<kMaxMsgSz> coded_reply = MultiMsg<kMaxMsgSz>::create_exact(sizeof(u8) + reply.size()).value();
        coded_reply.append(::rdmaio::Marshal::dump<u8>(CallStatus::Ok));
        coded_reply.append(reply);

        // send the reply to the client
        channel->reply_cur(coded_reply.buf);

        // move the buf back to the cur msg
        channel->cur() = std::move(segmeneted_msg.buf);

      } catch (std::exception &e) {
        // some error happens
        RDMA_LOG(4) << "error: " << e.what();
        channel->reply_cur(::rdmaio::Marshal::dump<u8>(CallStatus::Nop));
      }
    }
    return count;
  }
};

} // namespace bootstrap

} // namespace rdmaio
