#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h> //hostent
#include <unistd.h>

#include <map>

#include "../common.hh"
#include "./timer.hh"

namespace rdmaio {

const constexpr double notimeout = static_cast<double>(std::numeric_limits<int>::max());

/*!
 * Simple TCP server services for pre-connecting RDMA connections
 */
class SimpleTCP {
public:
  /*!
    given a "host:port", return (host,port)
   */
  static Option<std::pair<std::string, int>> parse_addr(const std::string &h) {
    auto pos = h.find(':');
    if (pos != std::string::npos) {
      std::string host_str = h.substr(0, pos);
      std::string port_str = h.substr(pos + 1);

      std::stringstream parser(port_str);

      int port = 0;
      if (parser >> port) {
        return std::make_pair(host_str, port);
      }
    }
    return {};
  }

  /*!
    Panic if failed to get one
   */
  static int get_listen_socket(const std::string &addr, int port) {

    struct sockaddr_in serv_addr;
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    RDMA_ASSERT(sockfd >= 0)
        << "ERROR opening listen socket: " << strerror(errno);

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // port
    serv_addr.sin_port = htons(port);

    // avoid TCP's TIME_WAIT state causing "ADDRESS ALREADY USE" Error
    int addr_reuse = 1;
    auto ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &addr_reuse,
                          sizeof(addr_reuse));
    RDMA_ASSERT(ret == 0);

    RDMA_ASSERT(
        bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0)
        << "ERROR on binding: " << strerror(errno);
    return sockfd;
  }

  static Result<std::pair<int, std::string>>
  accept_with_timeout(int socket, double timeout) {

    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);

      struct timeval tv = {.tv_sec = 0, .tv_usec = static_cast<int>(timeout)};
      auto ready = select(socket + 1, &rfds, nullptr, nullptr, &tv);

      if (ready < 0) { // error case
        return Err(std::make_pair(ready, std::string(strerror(errno))));
      }

      if (FD_ISSET(socket, &rfds)) {
        struct sockaddr_in cli_addr = {0};
        socklen_t clilen = sizeof(cli_addr);
        return Ok(std::make_pair(
                                 accept(socket, (struct sockaddr *)&cli_addr, &clilen), std::string("")));
      } else
        break;
    }
    return Timeout(std::make_pair(0, std::string("")));
  }

  /*!
    Alloc and connect a socket to the remote endpoint.
    Note: timeout in usec.
   */
  static Result<std::pair<int, std::string>>
  get_send_socket(const std::string &addr, int port,
                  const double &timeout = Timer::no_timeout()) {
    int sockfd = -1;
    struct sockaddr_in serv_addr;

    RDMA_ASSERT((sockfd = socket(AF_INET, SOCK_STREAM, 0)) >= 0)
        << "Error open socket for send!";
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    auto ip = host_to_ip(addr);
    if (ip == "") {
      close(sockfd);
      return Err(std::make_pair(-1,std::string("failed parse ip")));
    }

    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) ==
        -1) {
      if (errno == EINPROGRESS) {
        goto PROGRESS;
      }
      close(sockfd);
      return Err(std::make_pair(-1,std::string(strerror(errno))));
    }
  PROGRESS:
    // check return status
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    struct timeval tv = {.tv_sec = 0, .tv_usec = static_cast<int>(timeout)};
    if (select(sockfd + 1, nullptr, &fdset, nullptr, &tv) == 1) {
      int so_error;
      socklen_t len = sizeof so_error;

      getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

      if(FD_ISSET(sockfd,&fdset))
        goto ConnectOk;

      if (so_error != 0) {
        close(sockfd);
        return Err(std::make_pair(-1,std::string(strerror(errno))));
      }
    } else {
      // timeout case
      close(sockfd);
      return Timeout(std::make_pair(-1,std::string("")));
    }
 ConnectOk:
    return Ok(std::make_pair(sockfd,std::string("")));
  }

  static bool wait_recv(int socket, const double &timeout) {

    while (true) {

      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);

      struct timeval tv = {.tv_sec = 0, .tv_usec = static_cast<int>(timeout)};
      int ready = select(socket + 1, &rfds, nullptr, nullptr, &tv);

      if (ready == 0) { // no file descriptor found
        return false;   // timeout happens
      }

      if (ready < 0) { // error case
        // RDMA_ASSERT(false) << "select error " << strerror(errno);
        return false;
      }

      if (FD_ISSET(socket, &rfds)) {
        break; // ready
      }
    }
    return true;
  }

  static void wait_close(int socket) {

    shutdown(socket, SHUT_WR);
    char buf[2];

    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    auto ret = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                          (const char *)&tv, sizeof(tv));
    if (ret == 0)
      recv(socket, buf, 2, 0);
    close(socket);
  }

  static int send(int fd, const char *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = usrbuf;

    while (nleft > 0) {
      if ((nwritten = write(fd, bufp, nleft)) <= 0) {
        if (errno == EINTR) /* Interrupted by sig handler return */
          nwritten = 0;     /* and call write() again */
        else
          return -1; /* errno set by write() */
      }
      nleft -= nwritten;
      bufp += nwritten;
    }
    return n;
  }

  typedef std::map<std::string, std::string> ipmap_t;
  static ipmap_t &local_ip_cache() {
    static __thread ipmap_t cache;
    return cache;
  }

  static std::string host_to_ip(const std::string &host) {

    ipmap_t &cache = local_ip_cache();
    if (cache.find(host) != cache.end())
      return cache[host];

    std::string res = "";

    struct addrinfo hints, *infoptr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET means IPv4 only addresses

    int result = getaddrinfo(host.c_str(), nullptr, &hints, &infoptr);
    if (result) {
      fprintf(stderr, "getaddrinfo: %s at %s\n", gai_strerror(result),
              host.c_str());
      return "";
    }
    char ip[64];
    memset(ip, 0, sizeof(ip));

    for (struct addrinfo *p = infoptr; p != nullptr; p = p->ai_next) {
      getnameinfo(p->ai_addr, p->ai_addrlen, ip, sizeof(ip), nullptr, 0,
                  NI_NUMERICHOST);
    }

    res = std::string(ip);
    if (res != "")
      cache.insert(std::make_pair(host, res));
    return res;
  }
};

}; // namespace rdmaio
