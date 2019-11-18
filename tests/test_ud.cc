#include <gtest/gtest.h>

#include "../core/nicinfo.hh"

#include "../core/qps/recv_iter.hh"
#include "../core/qps/ud.hh"

#include "../core/utils/marshal.hh"

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

  auto ud = UD::create(nic, QPConfig()).value();
  ASSERT_TRUE(ud->valid());

  RDMA_LOG(4) << "passed ud create";

  // prepare the message buf
  auto mem =
      Arc<RMem>(new RMem(16 * 1024 * 1024)); // allocate a memory with 4M bytes
  ASSERT_TRUE(mem->valid());

  auto handler = RegHandler::create(mem, nic).value();
  SimpleAllocator alloc(mem, handler->get_reg_attr().value().key);

  // prepare buffer, contain 16 recv entries, each has 4096 bytes
  auto recv_rs = RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc);
  RDMA_LOG(2) << "recv rs: " << recv_rs.header;

  // post these recvs to the UD
  {
    auto res = ud->post_recvs(recv_rs, 2048);
    RDMA_ASSERT(res == IOCode::Ok);
  }

  /************************************/

  /** send the messages **/
  // 1. create the ah
  auto ah = ud->create_ah(ud->my_attr());
  ASSERT_NE(nullptr, ah);

  // 2. prepare the send request
  ibv_send_wr wr;
  ibv_sge sge;

  wr.opcode = IBV_WR_SEND_WITH_IMM;
  wr.num_sge = 1;
  wr.imm_data = 73;
  wr.next = nullptr;
  wr.sg_list = &sge;

  wr.wr.ud.ah = ah;
  wr.wr.ud.remote_qpn = ud->my_attr().qpn;
  wr.wr.ud.remote_qkey = ud->my_attr().qkey;
  wr.send_flags = IBV_SEND_INLINE | IBV_SEND_SIGNALED;

  // 3. send 1024 messages
  for (uint i = 0; i < 1024; ++i) {
    auto msg = ::rdmaio::Marshal::dump<u64>(i);
    sge.addr = (uintptr_t)(msg.data());
    sge.length = sizeof(u64);
    // no key is set in sge because the message is inlined

    struct ibv_send_wr *bad_sr = nullptr;
    auto ret = ud->send(wr, 1, &bad_sr);
    if (ret != IOCode::Ok)
      RDMA_LOG(4) << "post send error:" << strerror(ret.desc) << "; at: " << i;

    // wait one completion
    auto ret_r = ud->wait_one_comp();
    RDMA_ASSERT(ret_r == IOCode::Ok);
  }

  // start to recv message
  sleep(1);

  // finally we check the messages
  uint recved_msgs = 0;
  for (RecvIter iter(ud, recv_rs); iter.has_msgs(); iter.next()) {
    // check message content
    auto imm_msg = iter.cur_msg().value();
    auto buf = static_cast<char *>(std::get<1>(imm_msg));
    auto res = ::rdmaio::Marshal::dedump<u64>(buf);

    ASSERT_EQ(std::get<0>(imm_msg), 73);
    ASSERT_EQ(res, recved_msgs);

    recved_msgs += 1;
  }
  ASSERT_EQ(recved_msgs, 1024);
}

} // namespace test
