#pragma once

#include <iostream>

#include "./common.hh"
#include "./naming.hh"

namespace rdmaio {

class RNic {
  // context exposed by libibverbs
  struct ibv_context *ctx = nullptr;
  struct ibv_pd      *pd  = nullptr;
 public:
  const DevIdx id;

  /*!
    filled after a successful opened device
  */
  const Option<RAddress>     addr;
  const Option<u64>          lid;

  RNic(const DevIdx &idx,u8 gid = 0) :
      id(idx),
      ctx(open_device(idx)),
      pd(alloc_pd()),
      lid(fetch_lid(idx)),
      addr(query_addr(gid)) {

  }

  bool ready() const {
    return (ctx != nullptr) && (pd != nullptr);
  }

  struct ibv_context *get_ctx() {
    return ctx;
  }

  struct ibv_pd *get_pd() {
    return pd;
  }

  ~RNic() {
    // pd must he deallocaed before ctx
    if(pd != nullptr) {
      ibv_dealloc_pd(pd);
    }
    if(ctx != nullptr) {
      ibv_close_device(ctx);
    }
  }

  // implementations
 private:
  struct ibv_context *open_device(const DevIdx &idx) {

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

  struct ibv_pd *alloc_pd() {
    if(ctx == nullptr) return nullptr;
    auto ret = ibv_alloc_pd(ctx);
    if(ret == nullptr) {
      RDMA_LOG(WARNING) << "failed to alloc pd w error: " << strerror(errno);
    }
    return ret;
  }

  Option<u64> fetch_lid(const DevIdx &idx) {
    if(!ready()) {
      return {};
    } else {
      ibv_port_attr port_attr;
      auto rc = ibv_query_port(ctx, idx.port_id, &port_attr);
      if(rc >= 0)
        return Option<u64>(port_attr.lid);
      return {};
    }
  }

  Option<RAddress> query_addr(u8 gid_index = 0) const {

    if(!ready())
      return {};

    ibv_gid gid;
    ibv_query_gid(ctx,id.port_id,gid_index,&gid);

    RAddress addr {
      .subnet_prefix = gid.global.subnet_prefix,
      .interface_id  = gid.global.interface_id,
      .local_id      = gid_index
    };
    return Option<RAddress>(addr);
  }

};

} // namespace rdmaio
