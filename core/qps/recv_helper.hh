#pragma once

#include "./abs_recv_allocator.hh"

namespace rdmaio {

namespace qp {

/*!
  helper data struture for two-sided QP recv
 */
template <usize entries> struct RecvEntries {
  /*!
   internal data structure used for send/recv verbs
  */
  struct ibv_recv_wr rs[entries];
  struct ibv_sge sges[entries];
  struct ibv_wc wcs[entries];

  /*!
    current recv entry which points to one position in *rs*
  */
  usize header = 0;

  ibv_recv_wr *wr_ptr(const usize &idx) {
    // idx should be in [0,entries)
    return rs + idx;
  }

  ibv_recv_wr *header_ptr() {
    return wr_ptr(header);
  }
};

// AbsAllocator must inherit from *AbsRecvAllocator* defined in
// abs_recv_allocator.hh
template <class AbsAllocator, usize entries, usize entry_sz>
class RecvEntriesFactory {
public:
  inline static RecvEntries<entries> create(AbsAllocator &allocator) {
    RecvEntries<entries> ret;

    for (uint i = 0; i < entries; ++i) {
      auto recv_buf = allocator.alloc_one(entry_sz).value();

      struct ibv_sge sge = {
          .addr = reinterpret_cast<uintptr_t>(std::get<0>(recv_buf)),
          .length = entry_sz,
          .lkey = std::get<1>(recv_buf)};

      { // unsafe code
        ret.rs[i].wr_id = sge.addr;
        ret.rs[i].sg_list = &ret.sges[i];
        ret.rs[i].num_sge = 1;
        ret.rs[i].next = (i < entries) ? (&(ret.rs[i + 1])) : (&(ret.rs[0]));

        ret.sges[i] = sge;
      }
    }
    return ret;
  }
};

} // namespace qp
} // namespace rdmaio
