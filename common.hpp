/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
#include <tuple>
#include <infiniband/verbs.h>

#include "logging.hpp"

namespace rdmaio {

// connection status
enum IOStatus {
  SUCC         = 0,
  TIMEOUT      = 1,
  WRONG_ARG    = 2,
  ERR          = 3,
  NOT_READY    = 4,
  UNKNOWN      = 5,
  WRONG_ID     = 6,
  WRONG_REPLY  = 7
};

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

typedef std::tuple<std::string,int> MacID;

class QPDummy {
 public:
  bool valid() const {
    return qp_ != nullptr && cq_ != nullptr;
  }
 protected:
  struct ibv_qp  *qp_ = nullptr;
  struct ibv_cq *cq_  = nullptr;
}; // a placeholder for a dummy class


} // namespace rdmaio

#include "marshal.hpp"
#include "pre_connector.hpp"
#include "util.hpp"
