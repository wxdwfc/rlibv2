#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "./handler.hh"

namespace rdmaio {

namespace rmem {

using register_id_t = u64;

/*!
  The factory to manage all mr registeration.
  It check that all successful registered MR is valid.
 */
class RegFactory {
  std::map<register_id_t, Arc<RegHandler>> registered_mrs;
  std::mutex lock;

public:

  RegFactory() = default;

  ~RegFactory() {
    RDMA_LOG(2) << "reg factory drop(), total: " << registered_mrs.size()
                << " left mrs";
  }

  /*!
    Create a memory register handle and register it with the QP factory.
    \ret
    - Ok -> the registered MR
    - Err -> nothing, the error result may print to the screen
   */
  template <typename... Ts>
  Result<Arc<RegHandler>> create_and_reg(const register_id_t &mr_id,
                                         Ts... args) {
    auto reg_handler = Arc<RegHandler>(new RegHandler(args...));
    auto res = register_mr(mr_id, reg_handler);
    if (res == IOCode::Ok)
      return Ok(reg_handler);
    return Err(Arc<RegHandler>(nullptr));
  }

  /*!
    Register a mr to the factory identified by mr_id
    \ret: success: Ok
          error: if (res.desc) {
          // return already registered mr id
          }
          otherwise, mr is not valid, so it does not register
   */
  Result<Option<register_id_t>> register_mr(const register_id_t &mr_id,
                                            Arc<RegHandler> mr) {
    std::lock_guard<std::mutex> guard(lock);
    auto it = registered_mrs.find(mr_id);
    if (it != registered_mrs.end()) {
      return Err(Option<register_id_t>(it->first));
    }
    if (mr->valid()) {
      registered_mrs.insert(std::make_pair(mr_id, mr));
      return Ok(
          Option<register_id_t>(0)); // it's ok, so no need to pass descrption
    }
    return Err<Option<register_id_t>>({});
  }

  Option<Arc<RegHandler>> deregister_mr(const register_id_t &mr_id) {
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
  Option<RegAttr> get_attr_byid(const register_id_t &mr_id) {
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
