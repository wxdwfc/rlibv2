#pragma once

#include "../common.hh"

#include "../utils/ipname.hh"
#include "../utils/marshal.hh"

namespace rdmaio {

namespace bootstrap {

const usize kMaxMsgSz = 4096;

class AbsChannel {
protected:
  int sock_fd = -1;

  explicit AbsChannel(int sock) : sock_fd(sock) {}

  bool valid() const { return sock_fd >= 0; }

  Result<> close_channel() {
    if (valid()) {
      close(sock_fd);
      sock_fd = -1;
    }
    return Ok(); // we donot check the result here
  }

public:
  // It has to be abstract, otherwise shared_ptr cannot deallocate it
  ~AbsChannel() { close_channel(); }
};

/*!
  A UDP-based channel for sending msgs and recv msgs.
  Each msg is at maxinum ::rdmaio::bootstrap::kMaxMsgSz .
  An example:
  `
  auto sc = SendChannel::create("xx.xx.xx.xx:xx").value();
  auto send_res = sc.send("hello");
  ASSERT(send_res == IOCode::Ok);
  auto recv_res = sc.recv(1000); // recv with 1 second timeout
  ASSERT(recv_res == IOCode::Ok);
  `
 */
class SendChannel : public AbsChannel {
  explicit SendChannel(const std::string &ip, int port) {}

public:
  /*!
  Create a msg for the remote.
  The address is in the format (ip:port)
 */
  static Option<Arc<SendChannel>> create(const std::string &addr) {
    auto host_port = IPNameHelper::parse_addr(addr);
    if (host_port) {
      auto sc = Arc<SendChannel>(new SendChannel(
          std::get<0>(host_port.value()), std::get<1>(host_port.value())));
      if (sc->valid())
        return sc;
    }
    return {};
  }

  Result<std::string> send(const ByteBuffer &buf) {
    return Ok(std::string(""));
  }

  /*!
    Recv a reply of on the channel
   */
  Result<ByteBuffer> recv(usize timeout_ms = 0) {
    ByteBuffer res;
    return Ok(std::move(res));
  }
};

/*!
  A UDP-based channel for recving msgs.
  Each msg is at maxinum ::rdmaio::bootstrap::kMaxMsgSz .
  To use:
  `
  auto rc = RecvChannel::create (port_to_listen).value();
  for (rc->start(); rc->has_msg();rc->next()) {
    auto &msg = rc->cur();
    ...
    rc->reply_cur( some_reply);
  }
  `
 */
class RecvChannel : public AbsChannel {

  Option<int> cur_msg_client = {}; // who sent the current client
  ByteBuffer cur_msg;

  explicit RecvChannel(int port) {

    // TODO: init the sock_fd with UD
    // change it to non-blocking
    // resize cur_msg
  }

public:
  static Option<Arc<RecvChannel>> create(int port) { return {}; }

  /*!
    Try recv a msg;
    */
  void start() {
    // todo
  }

  bool has_msg() const {
    if (cur_msg_client)
      return true;
    return false;
  }

  /*!
    Drop current msg, and try to recv another one
   */
  bool next() {
    cur_msg_client = {}; // re-set msg header
    start();             // fill one msg
  }

  /*!
    \note: this call is not safe
   */
  ByteBuffer &cur() { return cur_msg; }

  Result<std::string> reply_cur(const ByteBuffer &buf){};

} // namespace bootstrap

} // namespace bootstrap
