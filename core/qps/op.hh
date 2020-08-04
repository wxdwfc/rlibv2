#pragma once

namespace rdmaio {

namespace qp {

using namespace rdmaio;
using namespace rdmaio::qp;

/*!
 Op states for single RDMA one-sided OP.
 This is a simple wrapper over the RC API provided by the RLib,
 which itself is a wrapper of libibverbs.

 Example usage:  // read 1 bytes at remote machine with address 0xc using
 one-sided RDMA.
      Arc<RC> qp; // some pre-initialized QP
      ::rdmaio::qp::Op op;
      op.set_rdma(rmr.buf + 0xc, rmr.key).set_read().set_imm(0);
      op.append_sge((u64)(lmr.buf), sizeof(u64), lmr.key)
      auto ret = op.execute(qp, IBV_SEND_SIGNALED);
 cas operation.
      op.set_cas(mr.buf, compare_data, swap_data, mr.key);
      op.append_sge((u64)(lmr.buf), sizeof(u64), lmr.key)
      auto ret = op.execute(qp, IBV_SEND_SIGNALED);
 faa operation.
      op.set_fetch_add(mr.buf, origin_data, mr.key);
      op.append_sge((u64)(lmr.buf), sizeof(u64), lmr.key)
      auto ret = op.execute(qp, IBV_SEND_SIGNALED);
 */
struct Op {
  ibv_send_wr wr;
  ibv_sge *sg_list;
  int sge_index;

 public:
  Op() {
    sge_index = 0;
    this->set_num_sge(1);
  }

  inline Op &set_op(const ibv_wr_opcode &op) {
    this->wr.opcode = op;
    return *this;
  }

  inline Op &set_read() {
    this->set_op(IBV_WR_RDMA_READ);
    return *this;
  }

  inline Op &set_write() {
    this->set_op(IBV_WR_RDMA_WRITE);
    return *this;
  }

  inline Op &set_rdma(const u64 &ra, const u32 &rk) {
    this->wr.wr.rdma.remote_addr = ra;
    this->wr.wr.rdma.rkey = rk;
    return *this;
  }

  inline Op &set_cas(const u64 &ra, const u64 &comp, const u64 &swap,
                     const u32 &rk) {
    this->set_atomic(ra, comp, swap, rk);
    this->set_op(IBV_WR_ATOMIC_CMP_AND_SWP);
    return *this;
  }

  inline Op &set_fetch_add(const u64 &ra, const u64 &add, const u32 &rk) {
    this->set_atomic(ra, add, 0, rk);
    this->set_op(IBV_WR_ATOMIC_FETCH_AND_ADD);
    return *this;
  }

  inline Op &set_atomic(const u64 &ra, const u64 &ca, const u64 &swap,
                        const u32 &rk) {
    this->wr.wr.atomic.remote_addr = ra;
    this->wr.wr.atomic.compare_add = ca;
    this->wr.wr.atomic.swap = swap;
    this->wr.wr.atomic.rkey = rk;
    this->set_imm(0);
    return *this;
  }

  inline Op &set_imm(const int &imm) {
    this->wr.imm_data = imm;
    return *this;
  }

  inline Op &set_num_sge(const int &num) {
    this->wr.num_sge = num;
    this->sg_list = (ibv_sge *)malloc(sizeof(ibv_sge) * num);
    return *this;
  }

  inline bool append_sge(const u64 &addr, const u32 &length,
                          const u32 &lkey) {
    if(this->wr.num_sge <= sge_index){
      return false;
    }

    this->sg_list[sge_index].addr = addr;
    this->sg_list[sge_index].length = length;
    this->sg_list[sge_index].lkey = lkey;

    sge_index ++;
    return true;
  }

  inline auto execute(const Arc<RC> &qp, const int &flags,
                      u64 wr_id = 0) -> Result<std::string> {
    // to avoid performance overhead of Arc, we first extract QP's raw pointer
    // out
    RC *qp_ptr = ({  // unsafe code
      RC *temp = qp.get();
      temp;
    });

    this->wr.wr_id = qp_ptr->encode_my_wr(wr_id, 1);
    this->wr.next = nullptr;
    this->wr.sg_list = this->sg_list;
    this->wr.send_flags = flags;

    if (this->wr.send_flags & IBV_SEND_SIGNALED) {
      qp_ptr->out_signaled += 1;
    }
    struct ibv_send_wr *bad_sr;
    auto res = ibv_post_send(qp_ptr->qp, &this->wr, &bad_sr);

    if (0 == res) {
      return Ok(std::string(""));
    }
    return Err(std::string(strerror(errno)));
  }
};
}  // qp

}  // namespace rdmaio
