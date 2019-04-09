#include "rdma_ctrl.hpp"
#include "rnic.hpp"

using namespace rdmaio;

int main() {

  // TODO: rnic should be dealloced before RdmaCtrl!
  {
    RdmaCtrl ctrl(0,8888);
    char *test_buffer = new char[64];

    RNic nic({.dev_id = 0,.port_id = 1});
    RDMA_LOG(2) << "nic " << nic.idx << " ready: " << nic.ready();

    RDMA_LOG(2) << ctrl.mr_factory.register_mr(73,test_buffer,64,nic);

    std::string req(sizeof(uint64_t),'0');
    *((uint64_t *)req.data()) = 73;

    auto res = ctrl.registered_handlers[1](req);
    RDMA_LOG(2) << res.capacity();
    ctrl.mr_factory.deregister_mr(73);
  }
}
