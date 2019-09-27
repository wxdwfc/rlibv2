#pragma once

#include <map>
#include <mutex>

#include "./rc.hh"

namespace rdmaio {

namespace qp {

class Factory {
  using register_id_t = u64;
  std::map<register_id_t, Arc<RC>> rc_store;

  std::mutex lock;
public:
  Factory() = default;

  /*!
    Register an RC qp to the QP factory with $id$.
    \ret
    - Ok -> nothing
    - Err -> if (res.desc) -> return the already registered QP id
    - Err -> if (!res.desc) -> the QP is not ready to register
   */
  Result<Option<register_id_t>> register_rc(const register_id_t &id,
                                            Arc<RC> rc) {
    if (!rc->valid())
      return Err(Option<register_id_t>(0));
    std::lock_guard<std::mutex> guard(lock);
    auto it = rc_store.find(id);
    if (it != rc_store.end()) {
      return Err(Option<register_id_t>(it->first));
    }
    rc_store.insert(std::make_pair(id, rc));
    return Ok(Option<register_id_t>(id));
  }

  /*!
    Create and register a QP, then register it to the factory
   */
  template <typename... Ts>
  Result<Arc<RC>> create_and_register_rc(const register_id_t &id, Ts... args) {
    auto qp = RC::create(args...);
    if (qp) {
      auto res = register_rc(id, qp.value());
      if (res == IOCode::Ok)
        return Ok(qp.value());
    }
    return Err(Arc<RC>(nullptr));
  }
};

} // namespace qps

} // namespace rdmaio
