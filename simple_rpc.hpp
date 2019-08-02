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

    IOStatus execute()
    {
        if (reqs.empty())
            return SUCC;
        // prepare the send payloads
        auto send_buf = Marshal::get_buffer(sizeof(ReqHeader));
        ReqHeader *header = (ReqHeader *)(send_buf.data()); // unsafe code
        for (uint i = 0; i < reqs.size(); ++i)
        {
            auto &req = reqs[i].first;
            header->elems[i].type = req.first;
            header->elems[i].payload = req.second.size();
            send_buf.append(req.second);
        }
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
