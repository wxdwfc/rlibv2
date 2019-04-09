#include "rnic.hpp"
#include "qp_config.hpp"

using namespace rdmaio;

int main() {

  RNic nic({.dev_id = 0,.port_id = 1});


  RDMA_LOG(2) << "This example prints the RNIC information on this machine.";

  auto res = RNicInfo::query_dev_names();
  RDMA_LOG(2) << "port num: " << res.size();
  for(auto i : res) {
    RDMA_LOG(2) << "get devs: " << i;
  }

  RCConfig config;
  config.clear_access_flags().add_access_read();
  RDMA_LOG(2) << config.desc_access_flags();

  RDMA_LOG(2) << "nic " << nic.idx << " ready: " << nic.ready();

  return 0;
}
