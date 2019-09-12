#pragma once

#include <cstring>
#include <memory>

#include "../nic.hh"
#include "./config.hh"

namespace rdmaio {

namespace rmem {

// TODO: change this to shared ptr
using RawMem = std::pair<char *, u64>; // pointer, payload

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
  std::shared_ptr<RNic> nic = std::makr_shared<RNic>(...);
  char *buffer = malloc(1024);
  RegHandler reg(std::make_pair(buffer,1024),rnic);
  Option<RegAttr> attr = reg.get_reg_attr();
  `
 */
class RegHandler {

  RawMem raw_mem;
  ibv_mr *mr = nullptr; // mr in the driver

  // a dummy reference to RNic, so that once the rnic is destoried otherwise,
  // its internal ib context is not destoried
  std::shared_ptr<RNic> rnic;

public:
  RegHandler(const RawMem &mem, const std::shared_ptr<RNic> &nic,
             const MemoryFlags &flags = MemoryFlags())
      : raw_mem(mem), rnic(nic) {

    if (rnic->ready()) {

      void *raw_ptr = (void *)(std::get<0>(raw_mem));
      u64 raw_sz = std::get<1>(raw_mem);

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
    return Option<RegAttr>({.buf = (uintptr_t)(std::get<0>(raw_mem)),
                            .sz = static_cast<u64>(std::get<1>(raw_mem)),
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
