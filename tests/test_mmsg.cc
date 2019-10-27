#include <gtest/gtest.h>

#include "../core/bootstrap/multi_msg.hh"

using namespace rdmaio;
using namespace rdmaio::bootstrap;

namespace test {

TEST(BootMsg, Basic) { MultBuffer msg_buf; }

  using TestMsg = MultiMsg<1024>;

  TestMsg mss; // create a MultioMsg with total 1024-bytes capacity.

  usize cur_sz = 0;
  for (uint i = 0; i < kMaxMultiMsg; ++i) {

    // todo: google test random string ?
    ByteBuffer msg;
    auto res = mss.append(msg);
    cur_sz += msg.size();

    if (!res) {
      ASSERT_TRUE(cur_sz + sizeof(MsgsHeader) > 1024);
      break;
    }
  }

  // now we parse the msg content
  auto copied_msg = mss.buf;
  auto mss_2 = TestMsg::create_from(copied_msg).value();
  // xx

  // iterate through the msgs to check contents

} // namespace test
