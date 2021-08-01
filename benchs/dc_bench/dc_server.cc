#include <gflags/gflags.h>
#include <sstream>
#include <vector>

#include "../../core/lib.hh"
#include "../../core/qps/op.hh"

#include "../../tests/random.hh"
#include "../reporter.hh"
#include "../thread.hh"

DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 73, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_int64(mem_sz, 4096, "Mr size");
DEFINE_int64(port, 8888, "Server listener (UDP) port.");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;

int
main(int argc, char** argv)
{
  RCtrl ctrl(FLAGS_port);

  RDMA_LOG(rdmaio::WARNING)
    << "Pingping server listenes at localhost:" << FLAGS_port;

  Arc<RNic> nic =
    RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();

  // >>>>>>>>>> server side <<<<<<<<<<<
  // remote dt node
  Arc<DCTarget> dc_node[2];
  dc_node[0] = DCTarget::create(nic, QPConfig()).value();

  ctrl.registered_dcs.reg("server_dc", dc_node[0]);
  RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_nic_name, nic));
  {
    // allocate a memory (with 1024 bytes) so that remote QP can access it
    RDMA_ASSERT(ctrl.registered_mrs.create_then_reg(
      FLAGS_reg_mem_name,
      Arc<RMem>(new RMem(1024 * 1024)),
      ctrl.opened_nics.query(FLAGS_reg_nic_name).value()));
  }
  ctrl.start_daemon();

  for (int i = 0; i < 600; ++i) {
    sleep(1);
  }
}
