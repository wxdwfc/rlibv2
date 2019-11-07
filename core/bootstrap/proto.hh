#pragma once

#include "../rmem/handler.hh"
#include "./srpc.hh"

namespace rdmaio {

namespace proto {

/*!
  RPC ids used for the callbacks
 */
enum RCtrlBinderIdType : ::rdmaio::bootstrap::rpc_id_t {
  Heartbeat = 0,
  FetchMr,
  CreateQp,
  Reserved,
};

enum CallbackStatus : u8 {
  Ok = 0,
  Err = 1,
  NotFound,
  WrongArg,
};

/*!
  Req/Reply for handling MR requests
 */
struct __attribute__((packed)) MRReq {
  rmem::register_id_t id;
  // maybe for future extensions, like permission, etc
};

struct __attribute__((packed)) MRReply {
  CallbackStatus status;
  RegAttr attr;
};

/*******************************************/

} // namespace proto

} // namespace rdmaio
