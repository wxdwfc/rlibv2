#pragma once

#include <map>
#include <mutex>

#include "./rc.hh"

namespace rdmaio {

namespace qps {

class Factory {
  using register_id_t = u64;
  std::map<register_id_t, Arc<RC>> rc_store;

  std::mutex lock;

  Factory() = default;

  /*!
    Register an RC qp to the QP factory with $id$.
    \ret
    - Ok -> nothing
    - Err -> if (res.desc) -> return the already registered QP id
    - Err -> if (!res.desc) -> the QP is not ready to register
   */
  Result<Option<u64>> register_rc(const register_id_t &id, Arc<RC> rc) {
    return Ok(Option<u64>(0));
  }
};

} // namespace qps

} // namespace rdmaio
