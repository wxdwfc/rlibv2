#pragma once

#include <iostream>

// net related
#include <netdb.h>

#include "./common.hh"

namespace rdmaio {

/*!
 */
using HostId = std::tuple<std::string, int>; // TCP host host,port

/*!
  DevIdx is a handy way to identify the RNIC.
  A NIC is identified by a dev_id, while each device has several ports.
*/
struct DevIdx {
  usize dev_id;
  usize port_id;

  friend std::ostream &operator<<(std::ostream &os, const DevIdx &i) {
    return os << "{" << i.dev_id << ":" << i.port_id << "}";
  }
};

/*!
  Internal network address used by the driver
 */
struct __attribute__((packed)) RAddress {
  u64 subnet_prefix;
  u64 interface_id;
  u32 local_id;
};

class NameHelper {
public:
  /*!
    given a "host:port", return (host,port)
   */
  static Option<std::pair<std::string, int>> parse_addr(const std::string &h) {
    auto pos = h.find(':');
    if (pos != std::string::npos) {
      std::string host_str = h.substr(0, pos);
      std::string port_str = h.substr(pos + 1);

      std::stringstream parser(port_str);

      int port = 0;
      if (parser >> port) {
        return std::make_pair(host_str, port);
      }
    }
    return {};
  }

  // XD: fixme, do we need a global lock here?
  static std::string inet_ntoa(struct in_addr addr) {
    return std::string(inet_ntoa(addr));
  }
};

} // namespace rdmaio
