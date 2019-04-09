/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
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
  WRONG_ID     = 6
};

struct ReplyHeader {
  uint16_t reply_status;
  uint16_t reply_payload;
};

typedef std::string Buf_t;
inline Buf_t get_buffer(int size) {
  return std::string(size,'0');
}

inline Buf_t null_reply() {
  auto res        = get_buffer(sizeof(ReplyHeader));
  auto reply_p    = (ReplyHeader *)(res.data());
  reply_p->reply_status  = ERR;
  reply_p->reply_payload = 0;
  return res;
}

class QPDummy {
 public:
  bool valid() const {
    return qp_ != nullptr && cq_ != nullptr;
  }
 protected:
  struct ibv_qp  *qp_ = nullptr;
  struct ibv_cq *cq_  = nullptr;
}; // a placeholder for a dummy class

// some utilities function
inline int convert_mtu(ibv_mtu type) {
  int mtu = 0;
  switch(type) {
    case IBV_MTU_256:
      mtu = 256;
      break;
    case IBV_MTU_512:
      mtu = 512;
      break;
    case IBV_MTU_1024:
      mtu = 1024;
      break;
    case IBV_MTU_2048:
      mtu = 2048;
      break;
    case IBV_MTU_4096:
      mtu = 4096;
      break;
  }
  return mtu;
}

} // namespace rdmaio
