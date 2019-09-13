#include <gtest/gtest.h>

#include "../core/nicinfo.hh"
#include "../core/rmem/factory.hh"

namespace test {

using namespace rdmaio;
using namespace rdmaio::rmem;

TEST(RMEM, can_reg) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine

  Arc<RNic> nic = Arc<RNic>(new RNic(res[0]));
  {
    // use RMem to allocate and manage a region of memory on the heap,
    // with size 1024
    auto mem = Arc<RMem>(new RMem(1024));
    // the allocation must be succesful
    ASSERT_TRUE(mem->valid());

    // register this rmem to the RDMA Nic specificed by the nic
    RegHandler handler(mem, nic);
    ASSERT_TRUE(handler.valid());
    ASSERT_TRUE(handler.get_reg_attr());

    auto mem_not_valid = Arc<RMem>(new RMem(
        1024, [](u64 sz) { return nullptr; }, [](RMem::raw_ptr_t p) {}));
    ASSERT_FALSE(mem_not_valid->valid());

    RegHandler handler_not_valid(mem_not_valid, nic);
    ASSERT_FALSE(handler_not_valid.valid());
  }
}

TEST(RMEM, factory) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine

  RegFactory factory;
  {
    // the nic's ownership will be passed to factory
    // with the registered mr.
    // so it will not free RDMA resource (ctx,pd) even it goes out of scope
    Arc<RNic> nic = Arc<RNic>(new RNic(res[0]));
    {
      auto mr = Arc<RegHandler>(new RegHandler(Arc<RMem>(new RMem(1024)), nic));
      auto res = factory.register_mr(73, mr);
      ASSERT_EQ(res.code.c, IOCode::Ok);

      // test that we filter out duplicate registeration.
      auto res1 = factory.register_mr(73, mr);
      ASSERT_EQ(res1.code.c, IOCode::Err);
      ASSERT_EQ(res1.desc.value(), 73);

      // test that an invalid MR should not be registered
      auto mem_not_valid = Arc<RMem>(new RMem(
          1024, [](u64 sz) { return nullptr; }, [](RMem::raw_ptr_t p) {}));
      ASSERT_FALSE(mem_not_valid->valid());

      auto mr1 = Arc<RegHandler>(new RegHandler(mem_not_valid, nic));
      auto res2 = factory.register_mr(12, mr1);
      ASSERT_EQ(res2.code.c, IOCode::Err);
      if (res2.desc) {
        ASSERT_TRUE(false); // should not go in there
      }
    }
  }
}

TEST(RMEM, Err) {
#if 0
  // an example to show the error case of not using Arc
  auto res = RNicInfo::query_dev_names();
  RNic nic(res[0]);
  char *buffer = new char[1024];
  auto mr = ibv_reg_mr(nic.get_pd(), buffer, 1024, MemoryFlags().get_value());
  ASSERT_NE(mr,nullptr);
  delete buffer;
#endif
}

} // namespace test
