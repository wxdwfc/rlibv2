#pragma once

#include "common.hpp"
#include "rnic.hpp"

#include <mutex>
#include <map>

namespace rdmaio {

class MemoryFlags {
 public:
  MemoryFlags() = default;

  MemoryFlags &set_flags(int flags) {
    protection_flags = flags;
    return *this;
  }

  int get_flags() const { return protection_flags; }

  MemoryFlags &clear_flags() {
    return set_flags(0);
  }

  MemoryFlags &add_local_write() {
    protection_flags |= IBV_ACCESS_LOCAL_WRITE;
    return *this;
  }

  MemoryFlags &add_remote_write() {
    protection_flags |= IBV_ACCESS_REMOTE_WRITE;
    return *this;
  }

  MemoryFlags &add_remote_read() {
    protection_flags |= IBV_ACCESS_REMOTE_READ;
    return *this;
  }

 private:
  int protection_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | \
      IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
};

/**
 * Simple wrapper over ibv_mr struct
 */
class RemoteMemory {
 public:
  RemoteMemory(const char *addr,uint64_t size,
               const RNic &rnic,
               const MemoryFlags &flags)
      : addr(addr),size(size) {
    if (rnic.ready()) {
      mr = ibv_reg_mr(rnic.pd,(void *)addr,size,flags.get_flags());
    }
  }

  bool valid() const {
    return (mr != nullptr);
  }

  ~RemoteMemory() {
    if(mr != nullptr)
      ibv_dereg_mr(mr);
  }

  struct Attr {
    uintptr_t  buf;
    uint32_t   key;
  };

  Attr get_attr() const {
    auto key = valid() ? mr->rkey : 0;
    return { .buf = (uintptr_t)(addr), .key = key };
  }

 private:
  const char    *addr = nullptr;
  uint64_t       size;
  ibv_mr         *mr = nullptr;    // mr in the driver
}; // class remote memory

/**
 * The MemoryFactor manages all registered memory of the system
 */
class RdmaCtrl;
class RMemoryFactory {
  friend class RdmaCtrl;
 public:
  RMemoryFactory() = default;
  ~RMemoryFactory() {
    for(auto it = registered_mrs.begin();it != registered_mrs.end();++it)
      delete it->second;
  }

  IOStatus register_mr(int mr_id,
                       const char *addr,int size,RNic &rnic,const MemoryFlags flags = MemoryFlags()) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(registered_mrs.find(mr_id) != registered_mrs.end())
      return WRONG_ID;
    RDMA_ASSERT(rnic.ready());
    registered_mrs.insert(std::make_pair(mr_id, new RemoteMemory(addr,size,rnic,flags)));
    RDMA_ASSERT(rnic.ready());

    if(registered_mrs[mr_id]->valid())
      return SUCC;

    registered_mrs.erase(registered_mrs.find(mr_id));
    return ERR;
  }

  void deregister_mr(int mr_id) {
    std::lock_guard<std::mutex> lk(this->lock);
    auto it = registered_mrs.find(mr_id);
    if(it == registered_mrs.end())
      return;
    delete it->second;
    registered_mrs.erase(it);
  }

  RemoteMemory *get_mr(int mr_id) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(registered_mrs.find(mr_id) != registered_mrs.end())
      return registered_mrs[mr_id];
  }

 private:
  std::map<int,RemoteMemory *>   registered_mrs;
  std::mutex lock;

 private:
  Buf_t get_mr_attr(uint64_t id) {
    std::lock_guard<std::mutex> lk(this->lock);
    if(registered_mrs.find(id) == registered_mrs.end()) {
      return "";
    }
    Buf_t res = get_buffer(sizeof(RemoteMemory::Attr));
    auto attr = registered_mrs[id]->get_attr();
    memcpy((void *)res.data(),&attr,sizeof(RemoteMemory::Attr));
    return res;
  }

  Buf_t get_mr_handler(const Buf_t &req) {
    if (req.size() != sizeof(uint64_t))
      return null_reply();
    auto reply = null_reply();
    auto reply_p    = (ReplyHeader *)(reply.data());

    auto mr_id = *((uint64_t *)(req.data()));
    auto mr = get_mr_attr(mr_id);

    if (mr.size() == 0)
      reply_p->reply_status = NOT_READY;
    else
      reply_p->reply_status = SUCC;
    reply.append(mr);
    return reply;
  }
};

} // end namespace rdmaio
