/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
#include <tuple>
#include <infiniband/verbs.h>

#include "logging.hpp"

namespace rdmaio {

// some constants definiations
// connection status
enum IOStatus {
  SUCC         = 0,
  TIMEOUT      = 1,
  WRONG_ARG    = 2,
  ERR          = 3,
  NOT_READY    = 4,
  UNKNOWN      = 5,
  WRONG_ID     = 6,
  WRONG_REPLY  = 7,
  NOT_CONNECT  = 8,
  EJECT        = 9,
  REPEAT_CREATE = 10
};

/**
 * Programmer can register simple request handler to RdmaCtrl.
 * The request can be bound to an ID.
 * This function serves as the pre-link part of the system.
 * So only simple function request handling is supported.
 * For example, we use this to serve the QP and MR information to other nodes.
 */
enum RESERVED_REQ_ID {
  REQ_RC = 0,
  REQ_UD = 1,
  REQ_UC = 2,
  REQ_MR = 3,
  FREE   = 4
};

enum {
  MAX_INLINE_SIZE = 64
};

/**
 * We use TCP/IP to identify the machine,
 * since RDMA requires an additional naming mechanism.
 */
typedef std::tuple<std::string,int> MacID;
inline MacID make_id(const std::string &ip,int port) {
  return std::make_tuple(ip,port);
}

class QPDummy {
 public:
  bool valid() const {
    return qp_ != nullptr && cq_ != nullptr;
  }
  struct ibv_qp  *qp_ = nullptr;
  struct ibv_cq *cq_  = nullptr;
  struct ibv_cq *recv_cq_ = nullptr;
}; // a placeholder for a dummy class

typedef struct {
  uint64_t subnet_prefix;
  uint64_t interface_id;
  uint32_t local_id;
} qp_address_t;

struct QPAttr {
  QPAttr(const qp_address_t &addr,int lid,int psn,int port_id,int qpn = 0,int qkey = 0):
      addr(addr),lid(lid),qpn(qpn),psn(psn),port_id(port_id){
  }
  QPAttr() {}
  qp_address_t addr;
  int lid;
  int psn;
  int port_id;
  int qpn;
  int qkey;
};

} // namespace rdmaio

#include "marshal.hpp"
#include "pre_connector.hpp"
