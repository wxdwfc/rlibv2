#include <gflags/gflags.h>

#include "../../core/cm.hh"

DEFINE_string(addr, "localhost:8888", "Server address to connect to.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 73, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
}
