#include <gtest/gtest.h>

#include "../core/qps/ud.hh"
#include "../core/cm_cmd/mod.hh"

namespace test {

TEST(Device, Basic) {

using namespace rdmaio::cmd;

 auto &dev = Device::get();
 ASSERT_TRUE(dev.valid());

 auto id0 = dev.alloc_handle().value();
 RDMA_LOG(4) << "alloc id: " << id0;
 RDMA_LOG(4) << "alloc id2: " << dev.alloc_handle().value();

  struct sockaddr_in      sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(8888);
  sin.sin_addr.s_addr = INADDR_ANY;

  dev.bind_addr(id0,sin);
  dev.query_addr(id0);
}

}
