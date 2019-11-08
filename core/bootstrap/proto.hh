#pragma once

#include "../rmem/factory.hh"

namespace rdmaio {

namespace proto {

using rpc_id_t = u8;

/*!
  RPC ids used for the callbacks
 */
enum RCtrlBinderIdType : rpc_id_t {
  HeartBeat,
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
  ::rdmaio::rmem::register_id_t id;
  // maybe for future extensions, like permission, etc
};

struct __attribute__((packed)) MRReply {
  CallbackStatus status;
  ::rdmaio::rmem::RegAttr attr;
};

/*******************************************/

} // namespace proto

} // namespace rdmaio
