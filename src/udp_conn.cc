#include "udp_conn.h"

namespace udp {

// creates a UDP socket or throws an exception on error.
Socket CreateSocket(int timeout_usec) {
  // create the socket
  Socket sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    throw net::SocketException();
  }

  // resuse the port immediately after the socket is killed.
  int optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                 sizeof(int))) {
    throw net::SocketException();
  }

  // set socket timeout if provided.
  if (timeout_usec > 0) {
    struct timeval timeout = {0, timeout_usec};
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                   sizeof(struct timeval))) {
      throw net::SocketException();
    }
  }

  return sockfd;
}

SocketAddress::SocketAddress(net::Address addr) {
  // get the remote server's DNS entry
  struct hostent *server = gethostbyname(addr.hostname().c_str());
  if (server == nullptr) {
    throw net::HostNotFoundException(addr.hostname());
  }

  // build the server's Internet address.
  addr_.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&addr_.sin_addr.s_addr,
        server->h_length);
  addr_.sin_port = htons(addr.port());
}

std::string SocketAddress::Hostname() const {
  struct hostent *hostp = gethostbyaddr((const char *)&addr_.sin_addr.s_addr,
                                        sizeof(addr_.sin_addr.s_addr), AF_INET);
  if (hostp == nullptr) {
    throw net::HostNotFoundException("");
  }

  std::string hostname(hostp->h_name);
  if (hostname == "localhost") {
    hostname = net::GetHostname();
  }
  return hostname;
}

unsigned short SocketAddress::Port() const { return ntohs(addr_.sin_port); }

void Client::Send(const char *buf, size_t size) {
  auto addr = remote_address_.addr();
  auto addrlen = remote_address_.addr_len();
  if (sendto(sockfd_, buf, size, 0, addr, addrlen) < 0) {
    throw net::SendException();
  }
}

void Client::SendWithAck(const char *buf, size_t size, unsigned int attempts,
                         OnReceiveFn validAck) {
  for (; attempts > 0; --attempts) {
    // Send the message to the client.
    Send(buf, size);

    // Create a zeroed out buffer to read the ack message into.
    char ackbuf[BUFSIZE];
    bzero(ackbuf, BUFSIZE);

    // receive from the socket.
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    int n = recvfrom(sockfd_, ackbuf, BUFSIZE, 0,
                     (struct sockaddr *)&clientaddr, &clientlen);

    // Check for error cases. This is either a timeout or some kind of
    // networking error. For timeouts, try sending the message again. For
    // anything else, throw an exception.
    if (n < 0) {
      bool isTimeout = errno == EAGAIN || errno == EWOULDBLOCK;
      if (isTimeout) {
        continue;
      } else {
        throw net::ReceiveException();
      }
    }

    if (validAck(shared_from_this(), ackbuf, n)) return;
  }
}

Server::Server(unsigned short port) : sockfd_(CreateSocket(0)) {
  // create a socket and associate the it with the port
  struct sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);

  if (bind(sockfd_, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    throw net::BindException();
  }
};

void Server::Listen(OnReceiveFn rcv) const {
  // While the server is running, wait for datagrams and
  // call the provided closure with their data.
  while (1) {
    // Create a zeroed out buffer to read the message into.
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);

    // receive from the socket.
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);
    int n = recvfrom(sockfd_, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr,
                     &clientlen);
    if (n < 0) {
      throw net::ReceiveException();
    }

    // call closure with new client.
    auto client = std::make_shared<udp::Client>(clientaddr);

    // Call the receive callback with the data received.
    bool keepRunning = rcv(client, buf, n);
    if (!keepRunning) break;
  }
}

}  // namespace udp
