#include <gtest/gtest.h>

#include "../core/nicinfo.hh"
#include "../core/rmem/factory.hh"

namespace test {

using namespace rdmaio;
using namespace rdmaio::rmem;

TEST(RMEM, can_reg) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine

  std::shared_ptr<RNic>  nic = std::make_shared<RNic>(res[0]);
  {
    char *buffer = (char *)malloc(1024);
    ASSERT_NE(buffer, nullptr);

    RegHandler handler(std::make_pair(buffer, 1024), nic);
    ASSERT_TRUE(handler.valid());
    ASSERT_TRUE(handler.get_reg_attr());

    RegHandler handler_not_valid(std::make_pair((char *)0, 1024), nic);
    ASSERT_FALSE(handler_not_valid.valid());
    delete buffer;
  }
}

TEST(RMEM, factory) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine

  RegFactory factory;
  std::shared_ptr<RNic>  nic = std::make_shared<RNic>(res[0]);
  {
    char *buffer = (char *)malloc(1024);
    ASSERT_NE(buffer, nullptr);

    auto mr = std::make_shared<RegHandler>(std::make_pair(buffer, 1024), nic);
    auto res = factory.register_mr(73, mr);
    ASSERT_EQ(res.code.c, IOCode::Ok);

    // test that we filter out duplicate registeration.
    auto res1 = factory.register_mr(73, mr);
    ASSERT_EQ(res1.code.c, IOCode::Err);
    ASSERT_EQ(res1.desc.value(), 73);

    // test that an invalid MR should not be registered
    auto mr1 =
        std::make_shared<RegHandler>(std::make_pair((char *)0, 1024), nic);
    auto res2 = factory.register_mr(12, mr1);
    ASSERT_EQ(res2.code.c, IOCode::Err);
    if (res2.desc) {
      ASSERT_TRUE(false); // should not go in there
    }

    delete buffer;
  }
}

TEST(RMEM,Err) {
#if 0
  // an example to show the error case of not using shared_ptr
  auto res = RNicInfo::query_dev_names();
  RNic nic(res[0]);
  char *buffer = new char[1024];
  auto mr = ibv_reg_mr(nic.get_pd(), buffer, 1024, MemoryFlags().get_value());
  ASSERT_NE(mr,nullptr);
  delete buffer;
#endif
}

} // namespace test
