#include <gflags/gflags.h>

#include <vector>

#include "../tests/random.hh"

#include "../core/lib.hh"
#include "./reporter.hh"
#include "./thread.hh"

using namespace rdmaio;  // warning: should not use it in a global space often
using namespace rdmaio::qp;
using namespace rdmaio::rmem;

using Thread_t = bench::Thread<usize>;

DEFINE_string(addr, "val09:8888", "Server address to connect to.");
DEFINE_int64(threads, 1, "#Threads used.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(para_factor, 20, "#keep <num> queries being processed.");
DEFINE_int64(reg_nic_name, 73, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");

usize worker_fn(const usize &worker_id, Statics *s);

bool volatile running = true;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<Thread_t *> workers;
  std::vector<Statics> worker_statics(FLAGS_threads);

  for (uint i = 0; i < FLAGS_threads; ++i) {
    workers.push_back(
        new Thread_t(std::bind(worker_fn, i, &(worker_statics[i]))));
  }

  // start the workers
  for (auto w : workers) {
    w->start();
  }

  Reporter::report_thpt(worker_statics, 10);  // report for 10 seconds
  running = false;                            // stop workers

  // wait for workers to join
  for (auto w : workers) {
    w->join();
  }

  RDMA_LOG(4) << "done";
}

usize worker_fn(const usize &worker_id, Statics *s) {

  ::test::FastRandom rand(0xdeadbeaf + worker_id);

  // 1. create a local QP to use
  auto nic =
      RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();
  auto qp = RC::create(nic, QPConfig()).value();

  // 2. create the pair QP at server using CM
  ConnectManager cm(FLAGS_addr);
  if (cm.wait_ready(1000000, 2) ==
      IOCode::Timeout)  // wait 1 second for server to ready, retry 2 times
    RDMA_ASSERT(false) << "cm connect to server timeout";

  auto qp_res =
      cm.cc_rc("thread-qp"+worker_id, qp, FLAGS_reg_nic_name, QPConfig());
  RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

  auto key = std::get<1>(qp_res.desc);
  RDMA_LOG(4) << "t-" << worker_id << " fetch QP authentical key: " << key;

  auto local_mem = Arc<RMem>(new RMem(1024 * 1024 * 20));  // 20M
  auto local_mr = RegHandler::create(local_mem, nic).value();

  auto fetch_res = cm.fetch_remote_mr(FLAGS_reg_mem_name);
  RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
  rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

  qp->bind_remote_mr(remote_attr);
  qp->bind_local_mr(local_mr->get_reg_attr().value());

  RDMA_LOG(4) << "t-" << worker_id << " started";
  u64 *test_buf = (u64 *)(qp->local_mr.value().buf);
  *test_buf = 0;

  u64 recv_cnt = 0, flying_cnt = 0, try_rounds = 5;
  while (running) {
    for (int i = 0; i < FLAGS_para_factor - flying_cnt; ++i) {
      compile_fence();
      int index = rand.next() % 10000;
      auto res_s = qp->send_normal(
          {.op = IBV_WR_RDMA_READ,
           .flags = IBV_SEND_SIGNALED,
           .len = sizeof(u64),
           .wr_id = worker_id},
          {.local_addr = reinterpret_cast<RMem::raw_ptr_t>(test_buf),
           .remote_addr = index * sizeof(u64),
           .imm_data = 0});
      RDMA_ASSERT(res_s == IOCode::Ok);
      s->increment();  // finish one request
    }

    for (int i = 0; i < try_rounds; i++) {
      auto res_p = qp->wait_one_comp(0);
      if (res_p == IOCode::Ok) {
        recv_cnt++;
      }
    }
    flying_cnt = s->data.counter - recv_cnt;
  }
  RDMA_LOG(4) << "t-" << worker_id << " stoped";
  while (recv_cnt < s->data.counter) {
    auto res_p = qp->wait_one_comp(0);
    if (res_p == IOCode::Ok) {
      recv_cnt++;
    }
  }
  cm.delete_remote_rc("thread-qp" + worker_id, key);
  return 0;
}
