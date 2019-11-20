#pragma once

#include <limits>

namespace rdmaio {

namespace qp {

/*!
  we restrict the maximun number of a doorbelled request to be 16
 */
const usize kNMaxDoorbell = 16;

/*!
  currently this class is left with no methods, because I found its hard
  to abstract many away the UD and RC doorbelled requests
*/
template <usize N> struct DoorbellHelper {
  ibv_send_wr srs[N];
  ibv_sge sges[N];

  const u8 cur_idx = 0;

  static_assert(std::numeric_limits<u8>::max() > kNMaxDoorbell,
                "Index type's max value must be larger than the number of max "
                "doorbell num");
};

} // namespace qp

} // namespace rdmaio
