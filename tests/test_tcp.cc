#include <gtest/gtest.h>

#include "../core/rpc/tcp.hh"
#include "../core/utils/marshal.hh"

namespace test {

using namespace rdmaio;

TEST(RPC, TCP) {

  auto addr = SimpleTCP::parse_addr("localhost:8888").value();
  auto listenfd =
      SimpleTCP::get_listen_socket(std::get<0>(addr), std::get<1>(addr));
  ASSERT_GT(listenfd, 0);

  int opt = 1;
  RDMA_VERIFY(ERROR,
              setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                         &opt, sizeof(int)) == 0)
      << "unable to configure socket status.";
  RDMA_VERIFY(ERROR, listen(listenfd, 1) == 0)
      << "TCP listen error: " << strerror(errno);

  auto res = SimpleTCP::get_send_socket(std::get<0>(addr), std::get<1>(addr),
                                        notimeout);
  ASSERT_EQ(res.code.c, IOCode::Ok);

  auto send_fd = std::get<0>(res.desc);

  u64 expected_send_sz = 1024 * 4;

  ByteBuffer send_buf = Marshal::alloc(expected_send_sz);
  for (uint i = 0; i < expected_send_sz; ++i) {
    send_buf[i] = 73 + i;
  }

  auto n = SimpleTCP::send(send_fd, send_buf.data(), send_buf.size());
  ASSERT_EQ(n, send_buf.size());

  // then check the recv
  auto res1 = SimpleTCP::accept_with_timeout(listenfd, notimeout);
  if(res1.code != IOCode::Ok) {
    RDMA_LOG(2) << "accept with error: " << std::get<1>(res1.desc);
  }

  ASSERT_EQ(res1.code.c,IOCode::Ok);
  auto csfd = std::get<0>(res1.desc);
  ASSERT_GT(csfd, 0);

  if (!SimpleTCP::wait_recv(csfd, notimeout))
    assert(false);

  ByteBuffer recv_buf = Marshal::alloc(expected_send_sz);
  n = recv(csfd, (void *)recv_buf.data(), recv_buf.size(), 0);
  ASSERT_EQ(n, send_buf.size());

  for (uint i = 0; i < expected_send_sz; ++i)
    ASSERT_EQ(send_buf[i], recv_buf[i]);

  // clean ups
  close(send_fd);

  SimpleTCP::wait_close(csfd);
  close(csfd);
  close(listenfd);

  RDMA_LOG(2) << "TCP basic done";
}

} // namespace test
