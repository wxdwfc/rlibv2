#pragma once

#include <cstring>
#include <memory>

#include "../nic.hh"
#include "./config.hh"
#include "./mem.hh"

namespace rdmaio {

namespace rmem {

/*!
  The attr that exchanged between nodes.
 */
struct __attribute__((packed)) RegAttr {
  uintptr_t buf;
  u64 sz;
  u32 key;
};

/*!
  RegHandler is a handler for register a block of continusous memory to RNIC.
  After a successful creation,
  one can get the attribute~(like permission keys) so that other QP can read the
  registered memory.

  Example:
  `
  std::Arc<RNic> nic = std::makr_shared<RNic>(...);
  auto mem = Arc<RMem>(1024);
  RegHandler reg(mem,rnic);
  Option<RegAttr> attr = reg.get_reg_attr();
  `
 */
class RegHandler {

  ibv_mr *mr = nullptr; // mr in the driver

  // a dummy reference to RNic, so that once the rnic is destoried otherwise,
  // its internal ib context is not destoried
  Arc<RNic> rnic;

  // record the <ptr,size> of the registered memory
  // the internal buffer will be dealloced when RMem is destoried
  Arc<RMem>   rmem;

public:
  RegHandler(const Arc<RMem> &mem, const Arc<RNic> &nic,
             const MemoryFlags &flags = MemoryFlags())
      : rmem(mem), rnic(nic) {

    if (rnic->ready()) {

      auto raw_ptr = rmem->raw_ptr;
      auto raw_sz = rmem->sz;

      mr = ibv_reg_mr(rnic->get_pd(), raw_ptr, raw_sz, flags.get_value());

      if (!valid()) {
        RDMA_LOG(4) << "register mr failed at addr: (" << raw_ptr << ","
                    << raw_sz << ")"
                    << " with error: " << std::strerror(errno);
      }
    }
    // end class init
  }

  bool valid() const { return mr != nullptr; }

  Option<RegAttr> get_reg_attr() const {
    if (!valid())
      return {};
    return Option<RegAttr>({.buf = (uintptr_t)(rmem->raw_ptr),
                            .sz = rmem->sz,
                            .key = mr->rkey});
  }

  ~RegHandler() {
    if (valid())
      ibv_dereg_mr(mr);
  }

  DISABLE_COPY_AND_ASSIGN(RegHandler);
};

} // namespace rmem

} // namespace rdmaio
