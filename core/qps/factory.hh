#pragma once

#include <map>
#include <mutex>
#include <stdlib.h> /* srand, rand */

#include "./rc.hh"

namespace rdmaio {

namespace qp {

class Factory {
  // TODO: shall we use a string to identify QPs?
public:
  using register_id_t = u64;

private:
  std::map<register_id_t, std::pair<Arc<RC>, u64>> rc_store;

  std::mutex lock;

public:
  Factory() = default;

  /*!
    Register an RC qp to the QP factory with $id$.
    \ret
    - Ok -> a randomly generate key
    - Err -> nothing
    - Err -> nothing
   */
  Result<u64> register_rc(const register_id_t &id, Arc<RC> rc) {
    if (!rc->valid())
      return Err(static_cast<u64>(0));
    std::lock_guard<std::mutex> guard(lock);
    auto it = rc_store.find(id);
    if (it != rc_store.end()) {
      return NearOk(static_cast<u64>(0));
    }

    // generate a non-zero key for the register
    u64 key = rand();
    while (key == 0)
      key = rand();

    rc_store.insert(std::make_pair(id, std::make_pair(rc, key)));
    return Ok(key);
  }

  /*!
    Create and register a QP, then register it to the factory
    // do we really need this call?
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

  /*!
    Query a registered (rc) QP attr
   */
  Option<Arc<RC>> query_rc(const register_id_t &id) {
    std::lock_guard<std::mutex> guard(lock);
    if (rc_store.find(id) != rc_store.end()) {
      return std::get<0>(rc_store[id]);
    }
    return {};
  }

  Option<Arc<RC>> deregister_rc(const register_id_t &id, const u64 &key) {
    std::lock_guard<std::mutex> guard(lock);
    auto it = rc_store.find(id);
    if (it != rc_store.end()) {
      if (key == std::get<1>(it->second)) {
        auto res = std::get<0>(it->second);
        rc_store.erase(it);
        return res;
      } else
        return Arc<RC>(nullptr);
    }
    return {}; // fail to register key due to authentication failure
  }
};

} // namespace qp

} // namespace rdmaio
