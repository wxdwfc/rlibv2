#pragma once

#include "common.hpp"
#include "qp_config.hpp"

namespace rdmaio {

// Track the out-going and acknowledged reqs
struct ReqProgress {
  uint64_t high_watermark = 0;
  uint64_t low_watermark  = 0;

  void out_one() {
    high_watermark += 1;
  }

  void done_one() {
    low_watermark += 1;
  }

  uint64_t pending_reqs() const {
    return high_watermark - low_watermark;
  }
};

class RCQP : public QPDummy {
 public:
  explicit RCQP(RNicHandler &rnic) {
    // TODO, not implemented
  }

 private:
  ReqProgress progress_;
}; // end class QP

} // end namespace rdmaio
