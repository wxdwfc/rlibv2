#pragma once

namespace rdmaio {

class Info {
 public:
  static std::string qp_addr_to_str(const qp_address_t &addr) {
    std::stringstream ostr("{");
    ostr << "subnet_prefix: " << addr.subnet_prefix << ";"
         << "interface_id:  " << addr.interface_id << ";"
         << "local_id:      " << addr.local_id << "}";
    return ostr.str();
  }
};

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

inline IOStatus send_request(const MacID &id, int req_type, const Buf_t &req,
                             Buf_t &reply,
                             const struct timeval &timeout) {
  IOStatus ret = SUCC;

  RequestHeader req_h = {.req_type = req_type, .req_payload = req.size() };

  auto req_buf = Marshal::serialize_to_buf(req_h);
  req_buf.append(req);

  // start to send
  auto socket = PreConnector::get_send_socket(std::get<0>(id),std::get<1>(id));
  if(socket < 0) {
    return ERR;
  }
  {
    auto n = send(socket,(char *)(req_buf.data()),req_buf.size(),0);
    if(n != req_buf.size())
      goto ERR_RET;
    if(!PreConnector::wait_recv(socket,timeout)) {
      ret = TIMEOUT; goto ERR_RET;
    }
    n = recv(socket,(char *)(reply.data()),reply.size(), MSG_WAITALL);
    if(n != reply.size()) {
      ret = WRONG_REPLY;goto ERR_RET;
    }
  }
ERR_RET:
  shutdown(socket,SHUT_RDWR);
  close(socket);
  return ret;
}

inline IOStatus poll_till_completion(ibv_cq *cq) {
  // TODO: implement
  return SUCC;
}

} // end namespace rdmaio
