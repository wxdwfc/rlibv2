#include <gtest/gtest.h>

#include "../core/lib.hh"

namespace test {

using namespace rdmaio;

TEST(CM, MR) {
  RDMA_LOG(4) << "ctrl started!";
  RCtrl ctrl(8888);
  ctrl.start_daemon();

  // now we register the MR to the CM

  // 1. open an RNic handler
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty()); // there has to be NIC on the host machine

  Arc<RNic> nic = Arc<RNic>(new RNic(res[0]));

  // 2. allocate a buffer, and register it
  // allocate a memory with 1024 bytes
  auto mr = Arc<RegHandler>(new RegHandler(Arc<RMem>(new RMem(1024)), nic));
  ctrl.registered_mrs.reg(73,mr);

  // 3. fetch it through ethernet
  ConnectManager cm("localhost:8888");
  if (cm.wait_ready(1000000,2) ==
      IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
    assert(false);

  rmem::RegAttr attr;
  auto fetch_res = cm.fetch_remote_mr(73,attr);
  RDMA_ASSERT(fetch_res == IOCode::Ok) << fetch_res.desc;

  // 4. check the remote fetched one is the same as the local copy
  auto local_mr = mr->get_reg_attr().value();
  ASSERT_EQ(local_mr.buf,attr.buf);
  ASSERT_EQ(local_mr.sz, attr.sz);
  ASSERT_EQ(local_mr.key,attr.key);

  ctrl.stop_daemon();
}

} // namespace test
