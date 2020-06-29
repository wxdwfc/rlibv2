#include <gflags/gflags.h>

#include <vector>

#include "../core/nicinfo.hh"
#include "../core/rctrl.hh"

#include "./thread.hh"

using namespace rdmaio; // warning: should not use it in a global space often

using Thread_t = bench::Thread<usize>;

DEFINE_int64(port, 8888, "Listener port.");
DEFINE_int64(threads, 1, "#Threads used.");

usize worker_fn(const usize &worker_id);

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  RCtrl ctrl(FLAGS_port);
  RDMA_LOG(4) << "(UD) Pingping server listenes at localhost:" << FLAGS_port;

  std::vector<Thread_t *> workers;

  for (uint i = 0;i < FLAGS_threads; ++i) {
    workers.push_back(new Thread_t(std::bind(worker_fn, i)));
  }

  // start the workers
  for (auto w : workers) {
    w->start();
  }

  // wait for workers to join
  for (auto w : workers) {
    w->join();
  }

  RDMA_LOG(4) << "done";
}

usize worker_fn(const usize &worker_id) {
  RDMA_LOG(4) << "Bench worker: " << worker_id << " started";
  return 0;
}
