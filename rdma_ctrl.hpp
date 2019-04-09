#pragma once

#include "memory.hpp"

#include <functional>

namespace rdmaio {

/**
 * Programmer can register simple request handler to RdmaCtrl.
 * The request can be bound to an ID.
 * This function serves as the pre-link part of the system.
 * So only simple function request handling is supported.
 * For example, we use this to serve the QP and MR information to other nodes.
 */
enum RESERVED_REQ_ID {
  REQ_QP = 0,
  REQ_MR = 1,
  FREE   = 2
};

class RdmaCtrl {
 public:
  RdmaCtrl(int node_id,int tcp_port,const std::string &ip = "localhost"):
      node_id_(node_id) {
    RDMA_ASSERT(register_handler(REQ_MR,std::bind(&RMemoryFactory::get_mr_handler,
                                                  &mr_factory,std::placeholders::_1)));
  }

  ~RdmaCtrl() = default;

  typedef std::function<Buf_t (const Buf_t &req)> req_handler_f;
  bool register_handler(int rid,req_handler_f f) {
    return check_with_insert(rid,registered_handlers,f);
  }

 public:
  const int      node_id_;
  RMemoryFactory mr_factory;

  //private:
  // registered services
  std::map<int,req_handler_f>        registered_handlers;

  std::mutex lock;

  template <typename  T>
  bool check_with_insert(int id,std::map<int,T> &m,const T &val) {
    std::lock_guard<std::mutex> lk(this->lock);
    if (m.find(id) == m.end()) {
      m.insert(std::make_pair(id,val));
      return true;
    }
    return false;
  }
};

} // end namespace rdmaio
