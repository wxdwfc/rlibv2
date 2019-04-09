#pragma once

#include "common.hpp"

#include <iostream>
#include <vector>

namespace rdmaio {

struct DevIdx {
  int dev_id;
  int port_id;

  friend std::ostream& operator<<(std::ostream& os, const DevIdx& i) {
    return os << "{" << i.dev_id << ":" << i.port_id << "}";
  }
};

class RNicInfo;
class RemoteMemory;

// The RNIC handler
class RNic {
  friend class RNicInfo;
  friend class RemoteMemory;
 public:
  RNic(DevIdx idx) :
      idx(idx),
      ctx(open_device(idx)),
      pd(alloc_pd(ctx)) {
  }

  bool ready() const {
    return (ctx != nullptr) && (pd != nullptr);
  }

  ~RNic() {
    // pd must he deallocaed before ctx
    if(pd != nullptr) {
      RDMA_VERIFY(INFO,ibv_dealloc_pd(pd) == 0)
          << "failed to dealloc pd at device " << idx
          << "; w error " << strerror(errno);
    }
    if(ctx != nullptr) {
      RDMA_VERIFY(INFO,ibv_close_device(ctx) == 0)
          << "failed to close device " << idx;
    }
  }

  // members and private helper functions
 public:
  const DevIdx idx;

 private:

  struct ibv_context *ctx = nullptr;
  struct ibv_pd      *pd  = nullptr;

  struct ibv_context *open_device(DevIdx idx) {

    ibv_context *ret = nullptr;

    int num_devices;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);
    if(idx.dev_id >= num_devices || idx.dev_id < 0) {
      RDMA_LOG(WARNING) << "wrong dev_id: " << idx << "; total " << num_devices <<" found";
      goto ALLOC_END;
    }

    RDMA_ASSERT(dev_list != nullptr);
    ret = ibv_open_device(dev_list[idx.dev_id]);
    if(ret == nullptr) {
      RDMA_LOG(WARNING) << "failed to open ib ctx w error: " << strerror(errno)
                        << "; at devid " << idx;
      goto ALLOC_END;
    }
 ALLOC_END:
    if(dev_list != nullptr)
      ibv_free_device_list(dev_list);
    return ret;
  }

  struct ibv_pd *alloc_pd(struct ibv_context *c) {
    if(c == nullptr) return nullptr;
    auto ret = ibv_alloc_pd(c);
    if(ret == nullptr) {
      RDMA_LOG(WARNING) << "failed to alloc pd w error: " << strerror(errno);
    }
    return ret;
  }
}; // end class RNic

/**
 * The following static methods describes the parameters of the devices.
 */
class RNicInfo {
  using Vec_t = std::vector<DevIdx>;
 public:
  static Vec_t query_dev_names() {
    Vec_t res;
    int num_devices;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);

    for(uint i = 0;i < num_devices;++i) {
      RNic rnic({.dev_id = i,.port_id = 73 /* a dummy value*/});
      if(rnic.ready()) {
        ibv_device_attr attr;
        auto rc = ibv_query_device(rnic.ctx, &attr);

        if (rc)
          continue;
        for(uint port_id = 1;port_id <= attr.phys_port_cnt;++port_id) {
          res.push_back({.dev_id = i,.port_id = port_id});
        }
      } else
        RDMA_ASSERT(false);
    }
    if(dev_list != nullptr)
      ibv_free_device_list(dev_list);
    return res;
  }

}; // end class RNicInfo

} // end namespace rdmaio
