#include <gtest/gtest.h>

#include "../core/qps/rc.hh"

namespace test {

using namespace rdmaio::qp;

TEST(RRC, basic) {

  auto config = QPConfig();
  ::rdmaio::qp::RC qp(config);
  ASSERT_TRUE(qp.valid());
}

} // namespace test
