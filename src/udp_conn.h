#ifndef UDP_CONN_H_
#define UDP_CONN_H_

#include <limits.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>

#include "net_exception.h"

typedef int Socket;
typedef struct sockaddr_in SocketAddress;

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
                   (const void *)&optval, sizeof(int))) {
        throw SocketException();
    }

    return sockfd;
}

class UdpClient {
    Socket socket;
    SocketAddress remote_address;
public:
    UdpClient(std::string hostname, int port) :
        socket(create_socket()), 
        remote_address(create_remote_address(hostname, port))
        {};

    ~UdpClient() { close(socket); };

    void send(const char* buf, size_t size) {
    //     /* send the message to the server */
    //     serverlen = sizeof(serveraddr);
    //     n = sendto(socket, buf, size, 0, &serveraddr, serverlen);
    //     if (n < 0) 
    //         error("ERROR in sendto");
    }
private:
    SocketAddress create_remote_address(std::string hostname, int port) {
        // get the remote server's DNS entry
        struct hostent* server = gethostbyname(hostname.c_str());
        if (server == nullptr) {
            throw HostNotFoundException(hostname);
        }

        // build the server's Internet address
        SocketAddress address;
        bzero((char *) &address, sizeof(address));
        address.sin_family = AF_INET;
        bcopy((char *) server->h_addr, 
              (char *) &address.sin_addr.s_addr, server->h_length);
        address.sin_port = htons(port);
        return address;
    }
};

class UdpServer {
    Socket socket;
public:
    UdpServer(int port) :
        socket(create_socket())
        {
            SocketAddress server_address = create_server_address(port);

            // associate the parent socket with a port
            if (bind(socket, (struct sockaddr *) &server_address, 
                sizeof(server_address)) < 0) {
                throw BindException();
            }


            // int clientlen; /* byte size of client's address */
            // struct sockaddr_in clientaddr; /* client addr */
            // struct hostent *hostp; /* client host info */
            // char buf[BUFSIZE]; /* message buf */
            // char *hostaddrp; /* dotted decimal host addr string */
            // int optval; /* flag value for setsockopt */
            // int n; /* message byte size */


            // /* 
            // * main loop: wait for a datagram, then echo it
            // */
            // clientlen = sizeof(clientaddr);
            // while (1) {

            //     /*
            //     * recvfrom: receive a UDP datagram from a client
            //     */
            //     bzero(buf, BUFSIZE);
            //     n = recvfrom(sockfd, buf, BUFSIZE, 0,
            //         (struct sockaddr *) &clientaddr, &clientlen);
            //     if (n < 0)
            //     error("ERROR in recvfrom");

            //     /* 
            //     * gethostbyaddr: determine who sent the datagram
            //     */
            //     hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
            //             sizeof(clientaddr.sin_addr.s_addr), AF_INET);
            //     if (hostp == NULL)
            //     error("ERROR on gethostbyaddr");
            //     hostaddrp = inet_ntoa(clientaddr.sin_addr);
            //     if (hostaddrp == NULL)
            //     error("ERROR on inet_ntoa\n");
            //     printf("server received datagram from %s (%s)\n", 
            //     hostp->h_name, hostaddrp);
            //     printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
                
            //     /* 
            //     * sendto: echo the input back to the client 
            //     */
            //     n = sendto(sockfd, buf, strlen(buf), 0, 
            //         (struct sockaddr *) &clientaddr, clientlen);
            //     if (n < 0) 
            //     error("ERROR in sendto");
            // }
        };
private:
    SocketAddress create_server_address(int port) {
        SocketAddress address;
        bzero((char *) &address, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons((unsigned short) port);
        return address;
    };
};

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

std::string get_hostname() {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}

#endif
