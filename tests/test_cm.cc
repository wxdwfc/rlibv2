#include <gtest/gtest.h>

#include "../core/rctrl.hh"

namespace test {

using namespace rdmaio;

TEST(CM, MR) {
  RDMA_LOG(4) << "ctrl started!";
  RCtrl ctrl(8888);
  ctrl.start_daemon();
  ctrl.stop_daemon();
}

} // namespace test
