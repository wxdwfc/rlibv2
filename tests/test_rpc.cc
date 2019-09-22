#include <gtest/gtest.h>

#include "../core/bootstrap/sender.hh"

namespace test {

using namespace rdmaio;

TEST(RPC, Sender) {
  auto addr = SimpleTCP::parse_addr("localhost:1111").value();
  SimpleRPC rpc(std::get<0>(addr), std::get<1>(addr));
  ASSERT_TRUE(rpc.valid());
}

} // namespace test
