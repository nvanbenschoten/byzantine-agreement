#ifndef UDP_CONN_H_
#define UDP_CONN_H_

#include <arpa/inet.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <limits.h>
#include <memory> 
#include <netdb.h> 
#include <netinet/in.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <thread>

#include "net_exception.h"

#define BUFSIZE 1024

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif


typedef int Socket;


Socket create_socket();
std::string get_hostname();


class SocketAddress {
 public:
	SocketAddress(struct sockaddr_in addr) : addr_(addr) {};
	SocketAddress(std::string host, int port);

	std::string hostname();

	const struct sockaddr* addr() { return (struct sockaddr*) &addr_; };
	const socklen_t addr_len() { return sizeof(addr_); };
private:
	struct sockaddr_in addr_;
};


class UdpClient {
 public:
	UdpClient(std::string hostname, int port) :
		sockfd_(create_socket()), 
		remote_address_(hostname, port) {};

	UdpClient(struct sockaddr_in addr) :
		sockfd_(create_socket()), 
		remote_address_(addr) {};

	~UdpClient() { close(sockfd_); };

	// sends the message to the remote server.
	void send(const char* buf, size_t size);

	// returns the hostname of the remote server.
	std::string remote_hostname() { return remote_address_.hostname(); };
 private:
	Socket sockfd_;
	SocketAddress remote_address_;
};


typedef std::function<void (UdpClient, char*)> on_receive;

class UdpServer {
 public:
	UdpServer(int port);
	~UdpServer();

	// listen forks a new thread to listen and call the provided
	// callback on.
	void listen(on_receive f);

	void respond(const SocketAddress client, const char* buf);
 private:
	Socket sockfd_;

	std::unique_ptr<std::thread> server_thread_;
	std::atomic<bool> running_;
};

#endif
