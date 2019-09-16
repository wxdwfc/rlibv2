#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "./handler.hh"

namespace rdmaio {

namespace rmem {

/*!
  The factory to manage all mr registeration.
  It check that all successful registered MR is valid.
 */
class RegFactory {
  std::map<u64, Arc<RegHandler>> registered_mrs;
  std::mutex lock;

public:
  RegFactory() = default;

  /*!
    Register a mr to the factory identified by mr_id
    \ret: success: Ok
          error: if (res.desc) {
          // return already registered mr id
          }
          otherwise, mr is not valid, so it does not register
   */
  Result<Option<u64>> register_mr(const u64 &mr_id, Arc<RegHandler> mr) {
    std::lock_guard<std::mutex> guard(lock);
    auto it = registered_mrs.find(mr_id);
    if (it != registered_mrs.end()) {
      return Err(Option<u64>(it->first));
    }
    if(mr->valid()) {
      registered_mrs.insert(std::make_pair(mr_id, mr));
      return Ok(Option<u64>(0)); // it's ok, so no need to pass descrption
    }
    return Err<Option<u64>>({});
  }

  Option<Arc<RegHandler>> deregister_mr(const u64 &mr_id) {
    std::lock_guard<std::mutex> guard(lock);
    auto it = registered_mrs.find(mr_id);
    if (it != registered_mrs.end()) {
      auto ret = Option<Arc<RegHandler>>(it->second);
      registered_mrs.erase(it);
      return ret;
    }
    return {};
  }

  /*!
    Query the attr by the id
   */
  Option<RegAttr> get_attr_byid(const u64 &mr_id) {
    std::lock_guard<std::mutex> guard(lock);
    auto it = registered_mrs.find(mr_id);
    if (it != registered_mrs.end()) {
      return it->second->get_reg_attr();
    }
    return {};
  }
};

} // namespace rmem

} // namespace rdmaio
