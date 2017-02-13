#include "udp_conn.h"

// creates a UDP socket or throws an exception on error.
Socket create_socket() {
  // create the socket
  Socket sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    throw SocketException();
  }

  // resuse the socket immediately after it is killed
  int optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                 (const void *) &optval, sizeof(int))) {
    throw SocketException();
  }

  return sockfd;
}

std::string get_hostname() {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  return std::string(hostname);
}

SocketAddress::SocketAddress(std::string host, int port) {
  // get the remote server's DNS entry
  struct hostent* server = gethostbyname(host.c_str());
  if (server == nullptr) {
    throw HostNotFoundException(host);
  }

  // build the server's Internet address.
  addr_.sin_family = AF_INET;
  bcopy((char *) server->h_addr, 
        (char *) &addr_.sin_addr.s_addr, server->h_length);
  addr_.sin_port = htons(port);
}

std::string SocketAddress::hostname() {
  struct hostent* hostp = gethostbyaddr((const char *) &addr_.sin_addr.s_addr, 
                                        sizeof(addr_.sin_addr.s_addr), AF_INET);
  if (hostp == nullptr) {
    throw HostNotFoundException("");
  }

  std::string hostname(hostp->h_name);
  if (hostname == "localhost") {
    hostname = get_hostname();
  }
  return hostname;
}

void UdpClient::send(const char* buf, size_t size) {
  auto addr = remote_address_.addr();
  auto addrlen = remote_address_.addr_len();
  if (sendto(sockfd_, buf, size, 0, addr, addrlen) < 0) {
    throw SendException();
  }
}

UdpServer::UdpServer(int port) : sockfd_(create_socket()) {
  // create a socket and associate the it with the port
  struct sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons((unsigned short) port);

  if (bind(sockfd_, (struct sockaddr *) &server_address, 
      sizeof(server_address)) < 0) {
    throw BindException();
  }
};

UdpServer::~UdpServer() {
  // if listen was ever called.
  if (server_thread_) {
    // switch the running flag to false
    running_ = false;

    // wait for the server thread to finish.
    server_thread_->join();
  }

  // close the socket.
  close(sockfd_);
};

void UdpServer::listen(on_receive f) {
  running_ = true;
  server_thread_ = std::make_unique<std::thread>([this, f](){
    // while the server is running, wait for datagrams and 
    // call the provided closure with their data.
    while (running_) {
      // zeroed out buffer to read message into.
      char buf[BUFSIZE];
      bzero(buf, BUFSIZE);

      // receive from the socket.
      struct sockaddr_in clientaddr;
      socklen_t clientlen = sizeof(clientaddr);
      int n = recvfrom(sockfd_, buf, BUFSIZE, 0,
                      (struct sockaddr *) &clientaddr, &clientlen);
      if (n < 0) {
        throw ReceiveException();
      }

      UdpClient client(clientaddr);

      // call closure with new client.
      f(client, buf);
    }
  });
}
