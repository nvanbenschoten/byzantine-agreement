#ifndef UDP_CONN_H_
#define UDP_CONN_H_

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "net.h"
#include "net_exception.h"

#define BUFSIZE 1024

namespace udp {

typedef int Socket;

Socket CreateSocket(int timeout_usec);

class SocketAddress {
 public:
  SocketAddress(struct sockaddr_in sockaddr) : addr_(sockaddr){};
  SocketAddress(net::Address addr);

  std::string Hostname() const;
  unsigned short Port() const;

  const struct sockaddr* addr() const { return (struct sockaddr*)&addr_; };
  const socklen_t addr_len() const { return sizeof(addr_); };

 private:
  struct sockaddr_in addr_;
};

class Client;
typedef std::shared_ptr<Client> ClientPtr;
typedef std::function<bool(ClientPtr, char*, size_t)> OnReceiveFn;

class Client : public std::enable_shared_from_this<Client> {
 public:
  Client(net::Address addr, long int timeout_usec = 0)
      : sockfd_(CreateSocket(timeout_usec)), remote_address_(addr){};

  Client(struct sockaddr_in sockaddr)
      : sockfd_(CreateSocket(0)), remote_address_(sockaddr){};

  ~Client() { close(sockfd_); };

  // Sends the message to the remote server.
  void Send(const char* buf, size_t size);

  // Sends the message to the remote server and waits for an acknowledgement.
  void SendWithAck(const char* buf, size_t size, unsigned int attempts,
                   OnReceiveFn validAck);

  // Returns the hostname of the remote server.
  net::Address RemoteAddress() const {
    return net::Address(remote_address_.Hostname(), remote_address_.Port());
  };
  std::string RemoteHostname() const { return remote_address_.Hostname(); };

 private:
  const Socket sockfd_;
  const SocketAddress remote_address_;
};

class Server {
 public:
  Server(unsigned short port);

  ~Server() { close(sockfd_); };

  void Listen(OnReceiveFn rcv) const;

 private:
  const Socket sockfd_;
};

}  // namespace udp

#endif
