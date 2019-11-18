#include <gtest/gtest.h>

#include "../core/qps/ud.hh"
#include "../core/qps/recv_helper.hh"
#include "../core/nicinfo.hh"

namespace test {

using namespace rdmaio;
using namespace rdmaio::qp;
using namespace rdmaio::rmem;

class SimpleAllocator : AbsRecvAllocator {
  RMem::raw_ptr_t buf = nullptr;
  usize total_mem = 0;
  mr_key_t key;

public:
  SimpleAllocator(Arc<RMem> mem, mr_key_t key)
      : buf(mem->raw_ptr), total_mem(mem->sz), key(key) {}

  Option<std::pair<rmem::RMem::raw_ptr_t, rmem::mr_key_t>>
  alloc_one(const usize &sz) override {
    if (total_mem < sz)
      return {};
    auto ret = buf;
    buf = static_cast<char *>(buf) + sz;
    total_mem -= sz;
    return std::make_pair(ret, key);
  }
};

TEST(UD, Create) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty());
  auto nic = std::make_shared<RNic>(res[0]);
  ASSERT_TRUE(nic->valid());

  auto ud = UD::create(nic,QPConfig()).value();
  ASSERT_TRUE(ud->valid());

  // prepare the message buf
  auto mem =
      Arc<RMem>(new RMem(4 * 1024 * 1024)); // allocate a memory with 4M bytes
  ASSERT_TRUE(mem->valid());

  auto handler = RegHandler::create(mem, nic).value();
  SimpleAllocator alloc(mem, handler->get_reg_attr().value().key);

  // prepare buffer
  auto recv_rs = RecvEntriesFactory<SimpleAllocator, 16, 4096>::create(alloc);
  RDMA_LOG(2) << "recv rs: " << recv_rs.header;

  // post these recvs to the UD

  /************************************/


  /** send the messages **/
}

} // namespace test
