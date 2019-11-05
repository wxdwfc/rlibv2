#include <gtest/gtest.h>

#include "../core/bootstrap/multi_msg.hh"

#include "./random.hh"

using namespace rdmaio;
using namespace rdmaio::bootstrap;

namespace test {

TEST(BootMsg, Basic) {

  using TestMsg = MultiMsg<1024>;

  TestMsg mss; // create a MultioMsg with total 1024-bytes capacity.

  FastRandom rand(0xdeadbeaf);

  usize cur_sz = 0;
  for (uint i = 0; i < kMaxMultiMsg; ++i) {

    // todo: google test random string ?
    ByteBuffer msg = rand.next_string(rand.rand_number<usize>(50, 300));
    auto res = mss.append(msg);
    cur_sz += msg.size();

    if (!res) {
      ASSERT_TRUE(cur_sz + sizeof(MsgsHeader) > 1024);
      break;
    }
  }

  RDMA_LOG(4) << "total " << mss.num_msg() << " test msgs added";

  // now we parse the msg content
  auto copied_msg = mss.buf;
  RDMA_LOG(4) << "copied msg's sz: " << copied_msg.size();
  auto mss_2 = TestMsg::create_from(copied_msg).value();
  ASSERT_EQ(mss_2.num_msg(),mss.num_msg());

  // iterate through the msgs to check contents
  RDMA_LOG(4) << "mss_2 buf: " <<(void *) mss_2.buf.data() << " copied: " << (void *)copied_msg.data()
              << "; origin: " << (void *)mss.buf.data();

  // finally we check the msg

}

} // namespace test
