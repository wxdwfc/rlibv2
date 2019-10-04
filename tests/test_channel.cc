#include <gtest/gtest.h>

#include "../core/bootstrap/channel.hh"

namespace test {

using namespace rdmaio;
using namespace rdmaio::bootstrap;

TEST(Channel, Naming) {
  auto host = "localhost";
  auto ip = IPNameHelper::host2ip(host);
  RDMA_ASSERT(ip == IOCode::Ok);
  RDMA_LOG(4) << "parsed ip: " << ip.desc;
  auto ip1 = IPNameHelper::host2ip("val02");
  RDMA_ASSERT(ip1 == IOCode::Ok);
  RDMA_LOG(4) << "parsed ip for val02: " << ip1.desc;

  auto ip3 = IPNameHelper::host2ip("  val02  ");
  RDMA_ASSERT(ip3 == IOCode::Ok)
      << "get ip3 code: " << ip3.code.name() << " " << ip3.desc;
  ASSERT_EQ(ip3.desc, ip1.desc);
}

TEST(Channel, Basic) {
  auto send_c = SendChannel::create("localhost:8888").value();
}

} // namespace test
