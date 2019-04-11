#pragma once

#include "rc.hpp"

#include <map>

namespace rdmaio {

class RdmaCtrl;
class QPFactory {
  friend class RdmaCtrl;
 public:
  QPFactory() = default;
  ~QPFactory() {
    for(auto it = rc_qps.begin();it != rc_qps.end();++it)
      delete it->second;
    for(auto it = ud_qps.begin();it != ud_qps.end();++it)
      delete it->second;
  }

  bool register_rc_qp(int id,RCQP *qp) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(rc_qps.find(id) != rc_qps.end())
      return false;
    rc_qps.insert(std::make_pair(id,qp));
    return true;
  }

  bool register_ud_qp(int id,RCQP *qp) {

  }

 private:
  std::map<int,RCQP *>   rc_qps;
  std::map<int,RCQP *>   ud_qps;

  // TODO: add UC QPs

  std::mutex lock;

  Buf_t get_rc_addr(uint64_t id) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(registered_mrs.find(id) == registered_mrs.end()) {
      return "";
    }
    auto attr = rc_qps[id]->addr;
    return Marshal::serialize_to_buf(attr);
  }

  Buf_t get_ud_addr(uint64_t id) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(registered_mrs.find(id) == registered_mrs.end()) {
      return "";
    }
    auto attr = rc_qps[id]->addr;
    return Marshal::serialize_to_buf(attr);
  }

  /** The RPC handler for the qp request
   * @Input = req:
   * - the address of QP the requester wants to fetch
   */
  Buf_t get_rc_handler(const Buf_t &req) {

    if (req.size() != sizeof(uint64_t))
      return Marshal::null_reply();

    uint64_t qp_id;
    bool res = Marshal::deserialize(req,mr_id);
    if(!res)
      return Marshal::null_reply();

    ReplyHeader reply = { .reply_status = SUCC,.reply_payload = sizeof(qp_address_t) };

    auto addr = get_rc_addr(qp_id);

    if(addr.size() == 0) {
      reply.reply_status = NOT_READY;
      reply.reply_payload = 0;
    }

    // finally generate the reply
    auto reply_buf = Marshal::serialize_to_buf(reply);
    reply_buf.append(addr);
    return reply_buf;
  }

  Buf_t get_ud_handler(const Buf_t &req) {

    if (req.size() != sizeof(uint64_t))
      return Marshal::null_reply();

    uint64_t qp_id;
    bool res = Marshal::deserialize(req,mr_id);
    if(!res)
      return Marshal::null_reply();

    ReplyHeader reply = { .reply_status = SUCC,.reply_payload = sizeof(qp_address_t) };

    auto addr = get_ud_addr(qp_id);

    if(addr.size() == 0) {
      reply.reply_status = NOT_READY;
      reply.reply_payload = 0;
    }

    // finally generate the reply
    auto reply_buf = Marshal::serialize_to_buf(reply);
    reply_buf.append(addr);
    return reply_buf;
  }

}; //

} // end namespace rdmaio
