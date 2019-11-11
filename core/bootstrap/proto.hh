#pragma once

#include "../rmem/factory.hh"
#include "../qps/factory.hh"
#include "../qps/config.hh"

namespace rdmaio {

namespace proto {

using rpc_id_t = u8;

/*!
  RPC ids used for the callbacks
 */
enum RCtrlBinderIdType : rpc_id_t {
  HeartBeat,
  FetchMr,
  CreateRC,
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

/*!
  Req/Reply for creating ~(RC) QPs
 */
struct __attribute__((packed)) RCReq {
  // parameter for querying the QP
  ::rdmaio::qp::Factory::register_id_t id;

  // parameter for creating the QP
  ::rdmaio::nic_id_t nic_id;
  u8 whether_create = 0; // 1: create the QP, 0 only query the QP attr
  ::rdmaio::qp::QPConfig  config;
  ::rdmaio::qp::QPAttr    attr; // the attr used for connect
};

struct __attribute__((packed)) RCReply {
  CallbackStatus status;
  ::rdmaio::qp::QPAttr attr;
};

/*******************************************/

} // namespace proto

} // namespace rdmaio
