#include <gtest/gtest.h>

#include "../core/qps/ud.hh"
#include "../core/nicinfo.hh"

namespace test {

using namespace rdmaio;

TEST(UD, Create) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty());
  auto nic = std::make_shared<RNic>(res[0]);
  ASSERT_TRUE(nic->valid());
}

} // namespace test
