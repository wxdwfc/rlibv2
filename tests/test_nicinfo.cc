#include <gtest/gtest.h>

#include "../core/nicinfo.hh"

TEST(RNic,Exists) {
  using namespace rdmaio;
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine
}
