#include "../../core/lib.hh"
#include "../../core/qps/op.hh"

#include "../../tests/random.hh"
#include "../reporter.hh"
#include "../thread.hh"
#include <gflags/gflags.h>
#include <sstream>
#include <vector>

DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_int64(mem_sz, 4096, "Mr size");
DEFINE_int64(dc_num, 1, "DC number");
DEFINE_int64(buf_size, 4096, "Server listener (UDP) port.");
DEFINE_int32(payload_sz, 8, "Server listener (UDP) port.");
DEFINE_int64(threads, 10, "#Threads used.");
DEFINE_string(machines,
              "localhost:8888",
              "remote machine list."); // split by one blank

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;
using namespace fLI64;

usize
worker_fn(const usize& worker_id, Statics* s);

using Thread_t = bench::Thread<usize>;
bool volatile running = true;

static inline ibv_ah*
create_ah(ibv_pd* pd, int lid)
{
  struct ibv_ah_attr ah_attr;
  ah_attr.is_global = 0;
  ah_attr.dlid = lid;
  ah_attr.sl = 0;
  ah_attr.src_path_bits = 0;
  ah_attr.port_num = 1;
  return ibv_create_ah(pd, &ah_attr);
}

inline std::vector<std::string>
split(const std::string& str, const std::string& delim)
{
  std::vector<std::string> res;
  if ("" == str)
    return res;
  char* strs = new char[str.length() + 1];
  strcpy(strs, str.c_str());

  char* d = new char[delim.length() + 1];
  strcpy(d, delim.c_str());

  char* p = strtok(strs, d);
  while (p) {
    std::string s = p;
    res.push_back(s);
    p = strtok(NULL, d);
  }

  return res;
}

int
main(int argc, char** argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<Thread_t*> workers;
  std::vector<Statics> worker_statics(FLAGS_threads); // setup workers

  for (uint i = 0; i < FLAGS_threads; ++i) {
    workers.push_back(
      new Thread_t(std::bind(worker_fn, i, &(worker_statics[i]))));
  }

  // start the workers
  for (auto w : workers) {
    w->start();
  }

  Reporter::report_thpt(worker_statics, 10); // report for 10 seconds
  running = false;                           // stop workers

  // wait for workers to join
  for (auto w : workers) {
    w->join();
  }

  RDMA_LOG(4) << "done";
}

usize
worker_fn(const usize& worker_id, Statics* s)
{
  ::test::FastRandom r(0xdeafbeaf + worker_id);

  Arc<RNic> nic =
    RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();
  // >>>>>>>>>> client side <<<<<<<<<<<
  std::vector<std::string> machines = split(FLAGS_machines, " ");
  int num_machine = machines.size();
  RDMA_ASSERT(num_machine > 0);

  std::vector<rmem::RegAttr> remote_regs(0);
  std::vector<DCAttr> dc_nodes(0);
  std::vector<ibv_ah*> address_handlers(0);

  RDMA_LOG(2) << "num remote machine:" << num_machine;

  // query remote infomation of each machine
  for (int i = 0; i < num_machine; ++i) {
    auto addr = machines[i];
    ConnectManager cm(addr);
    if (cm.wait_ready(1000000, 2) ==
        IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
      RDMA_ASSERT(false) << "cm connect to server timeout";

    auto fetch_qp_attr_res = cm.fetch_dc_attr("server_dc");
    RDMA_ASSERT(fetch_qp_attr_res == IOCode::Ok)
      << "fetch qp attr error: " << fetch_qp_attr_res.code.name() << " "
      << std::get<0>(fetch_qp_attr_res.desc);
    DCAttr attr = std::get<1>(fetch_qp_attr_res.desc);
    struct ibv_ah* ah = create_ah(nic->get_pd(), attr.lid);

    auto fetch_res = cm.fetch_remote_mr(FLAGS_reg_mem_name);
    RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
    rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

    dc_nodes.push_back(attr);
    address_handlers.push_back(ah);
    remote_regs.push_back(remote_attr);
  }
  RDMA_ASSERT(num_machine == dc_nodes.size());
  RDMA_ASSERT(num_machine == address_handlers.size());
  RDMA_ASSERT(num_machine == remote_regs.size());
  auto local_mem = Arc<RMem>(new RMem(FLAGS_mem_sz));
  auto local_mr = RegHandler::create(local_mem, nic).value();
  auto reg_attr = local_mr->get_reg_attr().value();

  std::vector<Arc<DC>> dcs(0);

  for (int i = 0; i < FLAGS_dc_num; ++i) {
    dcs.push_back(DC::create(nic, QPConfig()).value());
  }
  /* Use the remote LID to create an address handle for it */
  int mod = FLAGS_mem_sz / FLAGS_payload_sz;
  int dc_idx = 0;

  while (running) {
    dc_idx = (dc_idx + 1) % FLAGS_dc_num;
    compile_fence();
    u64 rand_val = r.next();
    u64 offset = rand_val % mod;

    int idx = rand_val % num_machine; // shift target
    compile_fence();
    auto send_res = dcs[dc_idx]->post_send(
      { .op = IBV_EXP_WR_RDMA_READ,
        .send_flags = IBV_EXP_SEND_SIGNALED,
        .ah = address_handlers[idx],
        .dct_num = dc_nodes[idx].dct_num,
        .dc_key = dc_nodes[idx].dc_key,
        .len = u32(FLAGS_payload_sz) },
      { .laddr = reg_attr.buf,
        .raddr = remote_regs[idx].buf + offset * FLAGS_payload_sz,
        .lkey = reg_attr.key,
        .rkey = remote_regs[idx].key });
    RDMA_ASSERT(send_res == IOCode::Ok);
    RDMA_ASSERT(dcs[dc_idx]->wait_one_comp() == IOCode::Ok);
    s->increment();
  }
}
