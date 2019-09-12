#pragma once

#include "../rmem/handler.hh"
#include "./config.hh"
#include "./mod.hh"

namespace rdmaio {

namespace qp {

class RC : public Dummy {

  // default local MR used by this QP
  Option<RegAttr> local_mr;
  // default remote MR used by this QP
  Option<RegAttr> remote_mr;

  // connect info used by others
  QPAttr my_attr;

  // pending requests monitor
  Progress progress;

  QPConfig my_config;

  RC(const QPConfig &config = QPConfig()) : my_config(config) {}

  int max_send_sz() const {
    return my_config.max_send_size;
  }
};

} // namespace qp

} // namespace rdmaio
