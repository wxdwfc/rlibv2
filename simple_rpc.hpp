#pragma once

#include <utility> // std::pair

#include "common.hpp"
#include "pre_connector.hpp"

namespace rdmaio
{

using reply_size_t = u16;
using req_size_t = u16;
struct ReqHeader
{
    using elem_t = struct
    {
        u16 type;
        u16 payload;
    };
    static constexpr const usize max_batch_sz = 4;
    elem_t elems[max_batch_sz];
    u8 total_reqs = 0;
};

struct ReplyHeader_
{
    reply_size_t reply_sizes[ReqHeader::max_batch_sz];
    u8 total_replies = 0;
};

/*!
a ReqResc = (req_type, req)
 */
using ReqDesc = std::pair<u8, Buf_t>;
/*!
    This is a very simple RPC upon socket.
    It is aim to handle bootstrap control path operation,
    so its performance is very slow.

    Example usage:
    Buf_t reply; 
    do {
        SimpleRPC sr("localhost",8888);
        if(!sr.valid()) {
            sleep(1);
            continue;
        }

        sr.emplace(REQ,"Hello",&reply);
        auto ret = sr.execute();
        if(ret == SUCC) {
            break;
        }
    } while(true);
    // deal with the reply ...

    \note: There are two limitation of this simple approach.
    First, the overall reply buf should be known at ahead. 
 */
class SimpleRPC
{
    using Req = std::pair<ReqDesc, Buf_t *>;
    int socket = -1;
    std::vector<Req> reqs;

public:
    SimpleRPC(const std::string &addr, int port)
        : socket(PreConnector::get_listen_socket(addr, port))
    {
    }

    /*!
    Emplace a req to the pending request list.
    \ret false: no avaliable batch entry
    \ret true:  in the list
     */
    bool emplace(const u8 &type, const Buf_t &req, Buf_t *reply)
    {
        if (reqs.size() == ReqHeader::max_batch_sz - 1)
            return false;
        reqs.push_back(std::make_pair(std::make_pair(type, req), reply));
        return true;
    }

    bool valid() const
    {
        return socket >= 0;
    }

    IOStatus execute(usize expected_reply_sz, const struct timeval &timeout)
    {
        if (reqs.empty())
            return SUCC;

        /*
        The code contains two parts.
        The first parts marshal the request to the buffer, and send it
        to the remote.
        The second part collect replies to their corresponding replies.
         */
        // 1. prepare the send payloads
        auto send_buf = Marshal::get_buffer(sizeof(ReqHeader));
        ReqHeader *header = (ReqHeader *)(send_buf.data()); // unsafe code

        for (uint i = 0; i < reqs.size(); ++i)
        {
            auto &req = reqs[i].first;
            header->elems[i].type = req.first;
            header->elems[i].payload = req.second.size();
            send_buf.append(req.second);
        }
        auto n = send(socket, (char *)(send_buf.data()), send_buf.size(), 0);
        if (n != reqs.size())
            return ERR;

        // wait for recvs
        if (!PreConnector::wait_recv(socket, timeout))
        {
            return TIMEOUT;
        }

        // 2. the following handles replies
        Buf_t reply = Marshal::get_buffer(expected_reply_sz + sizeof(ReplyHeader_));
        n = recv(socket, (char *)(reply.data()), reply.size(), MSG_WAITALL);
        if (n < reply.size())
            return WRONG_ARG;

        // now we parse the replies to fill the reqs
        const auto &reply_h = *(reinterpret_cast<const ReplyHeader_ *>(reply.data()));
        if (reply_h.total_replies < reqs.size())
            return WRONG_ARG;
        usize parsed_replies = 0;
        for (uint i = 0; i < reply_h.total_replies; ++i)
        {
            // sanity check replies size
            if (parsed_replies + reply_h.reply_sizes[i] >
                reply.size() - sizeof(ReplyHeader_))
            {
                RDMA_LOG(4) << "Wrong reply size.";
                return WRONG_ARG;
            }
            // append the corresponding reply to the buffer
            reqs[i].second->append(reply.data() + sizeof(ReplyHeader_) + parsed_replies,
                                   reply_h.reply_sizes[i]);
            parsed_replies += reply_h.reply_sizes[i];
        }
        return SUCC;
    }

    ~SimpleRPC()
    {
        if (valid())
        {
            shutdown(socket, SHUT_RDWR);
            close(socket);
        }
    }
}; // namespace rdmaio

} // namespace rdmaio
