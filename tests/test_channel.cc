#include <gtest/gtest.h>

#include "../core/bootstrap/channel.hh"

namespace test {

using namespace rdmaio;
using namespace rdmaio::bootstrap;

TEST(Channel, Basic) {
  auto send_c = SendChannel::create("localhost:8888").value();

}

} // namespace test
