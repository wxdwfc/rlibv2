#include <gtest/gtest.h>

#include "../core/nicinfo.hh"
#include "../core/qps/rc.hh"

namespace test {

using namespace rdmaio::qp;
using namespace rdmaio;

TEST(RRC, basic) {

  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty());
  auto nic = std::make_shared<RNic>(res[0]);
  ASSERT_TRUE(nic->valid());

  auto config = QPConfig();
  auto qp = RC::create(nic,config).value();
  ASSERT_TRUE(qp->valid());
}

} // namespace test
