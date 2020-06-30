#include <gflags/gflags.h>

#include <vector>

#include "../core/nicinfo.hh"
#include "../core/rctrl.hh"

#include "./thread.hh"

#include "./reporter.hh"

using namespace rdmaio; // warning: should not use it in a global space often

using Thread_t = bench::Thread<usize>;

DEFINE_int64(port, 8888, "Listener port.");
DEFINE_int64(threads, 1, "#Threads used.");

usize worker_fn(const usize &worker_id, Statics *);

bool volatile running = true;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  RCtrl ctrl(FLAGS_port);
  RDMA_LOG(4) << "(UD) Pingping server listenes at localhost:" << FLAGS_port;

  std::vector<Thread_t *> workers;
  std::vector<Statics>    worker_statics(FLAGS_threads);

  for (uint i = 0; i < FLAGS_threads; ++i) {
    workers.push_back(
        new Thread_t(std::bind(worker_fn, i, &(worker_statics[i]))));
  }

  // start the workers
  for (auto w : workers) {
    w->start();
  }

  Reporter::report_thpt(worker_statics, 10); // report for 10 seconds
  running = false; // stop workers

  // wait for workers to join
  for (auto w : workers) {
    w->join();
  }

  RDMA_LOG(4) << "done";
}

usize worker_fn(const usize &worker_id, Statics *s) {
  RDMA_LOG(4) << "Bench worker: " << worker_id << " started";

  while (running) {
    compile_fence();
    sleep(1);
    s->increment(); // finish one request
  }
  return 0;
}
