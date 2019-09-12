#pragma once

#include <exception>

#include "./proto.hh"

namespace rdmaio {

class RPCFactory
{
  /*!
  A simple RPC function:
  handle(const ByteBuffer &req) -> ByteBuffer
   */
  using req_handler_f = std::function<ByteBuffer(const ByteBuffer& req)>;
  std::map<int, req_handler_f> registered_handlers;

public:
  bool register_handler(int id, req_handler_f val)
  {
    if (registered_handlers.find(id) == registered_handlers.end()) {
      registered_handlers.insert(std::make_pair(id, val));
      return true;
    }
    return false;
  }

  ByteBuffer handle_one(int socket)
  {
    ByteBuffer buf = Marshal::get_buffer(4096);
    auto n = recv(socket, (char*)(buf.data()), 4096, 0);
    if (n < sizeof(ReqHeader)) {
      return null_reply();
    }
    ReqHeader header = Marshal::deserialize<ReqHeader>(buf);
    ByteBuffer reply = Marshal::get_buffer(sizeof(ReplyHeader_));

    RDMA_ASSERT(header.total_reqs <= ReqHeader::max_batch_sz)
      << "get total reqs: " << (int)header.total_reqs;

    buf = Marshal::direct_forward(buf, sizeof(ReqHeader));

    std::vector<usize> reply_sz;
    for (uint i = 0; i < header.total_reqs; ++i) {
      const auto& reqdesc = header.elems[i];
      try {
        auto temp = registered_handlers[reqdesc.type](buf);
        reply_sz.push_back(temp.size());
        reply.append(temp);
        buf = Marshal::direct_forward(buf, reqdesc.payload);
      } catch (std::exception& e) {
        // pass
        RDMA_ASSERT(false); // FIXME!
      }
    }

    ReplyHeader_* rheader = (ReplyHeader_*)(reply.data()); // unsafe code
    for (uint i = 0; i < reply_sz.size(); ++i) {
      rheader->reply_sizes[i] = reply_sz[i];
    }
    rheader->total_replies = reply_sz.size();

    return reply;
  }
};

} // namespace rdmaio
