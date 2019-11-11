#include <gflags/gflags.h>

#include "../../core/lib.hh"

DEFINE_string(addr, "localhost:8888", "Server address to connect to.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 73, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);


  // create a local QP to use
  auto nic = RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();
  auto qp = RC::create(nic, QPConfig()).value();

  // create the pair QP at server using CM
  ConnectManager cm(FLAGS_addr);
  if (cm.wait_ready(1000000, 2) ==
      IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
    RDMA_ASSERT(false) << "cm connect to server timeout";

  u64 key = 0;
  auto qp_res = cm.cc_rc(73, qp, key, FLAGS_reg_nic_name, QPConfig());
  RDMA_ASSERT(qp_res == IOCode::Ok) << qp_res.desc;
  RDMA_LOG(4) << "client fetch QP authentical key: " << key;

  // finally, some clean up, to delete my created QP at server
  auto del_res = cm.delete_remote_rc(73,key);
  RDMA_ASSERT(del_res == IOCode::Ok) << "delete remote QP error: "<< del_res.desc;

  RDMA_LOG(4) << "client returns";

  return 0;
}
