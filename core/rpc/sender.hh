#pragma once

#include "./proto.hh"
#include "./tcp.hh"

namespace rdmaio {

/*!
    This is a very simple RPC upon socket.
    It is aim to handle bootstrap control path operation,
    so its performance is very slow.

    Example usage:
    ByteBuffer reply;
    do {
        SimpleRPC sr("localhost",8888);
        if(!sr.valid()) {
            sleep(1);
            continue;
        }

        sr.emplace(REQ,"Hello",&reply);
        auto ret = sr.execute();
        if(ret.code = Ok) {
            break;
        }
    } while(true);
    // deal with the reply ...

    \note: There are two limitation of this simple approach.
    First, the overall reply buf should be known at ahead.
 */
class SimpleRPC {
  using Req = std::pair<ReqDesc, ByteBuffer *>;
  int socket = -1;
  std::vector<Req> reqs;

public:
  SimpleRPC(const std::string &addr, int port)
      : socket(SimpleTCP::get_send_socket(addr, port)) {}

  /*!
  Emplace a req to the pending request list.
  \ret false: no avaliable batch entry
  \ret true:  in the list
   */
  bool emplace(const u8 &type, const ByteBuffer &req, ByteBuffer *reply) {
    if (reqs.size() == ReqHeader::max_batch_sz - 1)
      return false;
    reqs.push_back(std::make_pair(std::make_pair(type, req), reply));
    return true;
  }

  bool valid() const { return socket >= 0; }

  Result<std::string> execute(usize expected_reply_sz, const double &timeout) {
    if (reqs.empty())
      return Ok(std::string(""));

    /*
    The code contains two parts.
    The first parts marshal the request to the buffer, and send it
    to the remote.
    The second part collect replies to their corresponding replies.
     */
    // 1. prepare the send payloads
    auto send_buf = Marshal::alloc(sizeof(ReqHeader));

    for (auto &req : reqs) {
      send_buf.append(req.first.second);
    }

    ReqHeader *header = (ReqHeader *)(send_buf.data()); // unsafe code
    for (uint i = 0; i < reqs.size(); ++i) {
      auto &req = reqs[i].first;
      header->elems[i].type = req.first;
      header->elems[i].payload = req.second.size();
    }

    header->total_reqs = static_cast<u8>(reqs.size());
    asm volatile("" :: // a compile fence
                 : "memory");
    auto n = send(socket, (char *)(send_buf.data()), send_buf.size(), 0);
    if (n != send_buf.size()) {
      return Err(std::string("send size error"));
    }
    // wait for recvs
    if (!SimpleTCP::wait_recv(socket, timeout)) {
      return Timeout(std::string(""));
    }

    // 2. the following handles replies
    auto reply = Marshal::alloc(expected_reply_sz + sizeof(ReplyHeader));

    n = recv(socket, (char *)(reply.data()), reply.size(), MSG_WAITALL);
    if (n < reply.size()) {
      return Err(std::string("wrong recv sz"));
    }

    // now we parse the replies to fill the reqs
    const auto &reply_h =
        *(reinterpret_cast<const ReplyHeader_ *>(reply.data()));

    if (reply_h.total_replies < reqs.size()) {
      return Err(std::string("wrong recv num"));
    }
    usize parsed_replies = 0;

    for (uint i = 0; i < reply_h.total_replies; ++i) {
      // sanity check replies size
      if (parsed_replies + reply_h.reply_sizes[i] >
          reply.size() - sizeof(ReplyHeader_)) {
        return Err(std::string("wrong recv payload"));
      }
      // append the corresponding reply to the buffer
      reqs[i].second->append(reply.data() + sizeof(ReplyHeader_) +
                                 parsed_replies,
                             reply_h.reply_sizes[i]);
      parsed_replies += reply_h.reply_sizes[i];
    }
    return Ok(std::string(""));
  }

  ~SimpleRPC() {
    if (valid()) {
      shutdown(socket, SHUT_RDWR);
      close(socket);
    }
  }
};

} // namespace rdmaio
