#pragma once

#include "./nicinfo.hh"
/*!
  Utilities for query RDMA NIC on this machine.

  Example usage:
  1) query all avaliable NICs, and return a vector of RLib's internal naming of
  these nics
  `
  std::vector<DevIdx> nics = RNicInfo::query_dev_names();
  `

  One can open the nic using the queried names:
  `
  auto nic =
  RNic::create(RNicInfo::query_dev_names().at(0).value();
  // using the first queried nic name
  `
 */

#include "./rctrl.hh"

#include "./cm.hh"
