#include <gtest/gtest.h>

#include "../core/utils/marshal.hh"

TEST(RDMAIO,Marshal) {

  using namespace rdmaio;

  // init the test buffer
  usize test_sz = 12;

  U8buf test_buf = Marshal::alloc_buf(test_sz);
  for(u8 i = 0;i < test_sz;++i) {
    //Marshal::safe_set_byte(test_buf,i,i);
    test_buf[i] = i;
  }

  auto res = Marshal::forward(test_buf, 0);
  usize count = 0;
  while(res && count < test_sz) {

    ASSERT_EQ(res.value()[0],count);
    count += 1;

    res = Marshal::forward(res.value(), 1);
  }
  ASSERT_EQ(count, test_sz);
}
