#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/types.h>
#include <sys/poll.h>
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/SocketException.h"
#include "socket_wrapper/BaseTypes.h"
namespace socket_wrapper {

    std::array<Stream,2> StreamFactory::CreatePipe(){
        int fds[2];
        if(socketpair(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP, fds)){
            throw SocketException(SocketException::SOCKET_PAIR, errno);
        }
        return {Stream(fds[0]),Stream(fds[1])};
    }
    Stream StreamFactory::CreateTcpStreamToServer(std::string ip_address, uint16_t port, IP_VERSION version) {
        int client_socket_fd;
        if(version == IP_VERSION::IPv6){
            struct sockaddr_in6 server_addr;
            if ((client_socket_fd = socket(AF_INET6, SOCK_STREAM , IPPROTO_TCP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            bzero(&server_addr, sizeof(server_addr));
            // assign IP, PORT
            server_addr.sin6_family = AF_INET6;
            inet_pton(AF_INET6, ip_address.c_str(), &server_addr.sin6_addr);
            server_addr.sin6_port = htons(port);

            /*// wait until connect is available (required for support for non blocking sockets
            std::array<pollfd, 1> poll_fds = {{{.fd = client_socket_fd, .events = POLLIN, .revents = 0}}};
            if (poll(poll_fds.data(), poll_fds.size(), -1) == -1) {
                throw std::logic_error("error while poll'ing for client");
            }*/
            // connect the client socket to server socket
            if (::connect(client_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                throw SocketException(SocketException::SOCKET_CONNECT, errno);
            }
        }else{
            struct sockaddr_in server_addr;
            if ((client_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            bzero(&server_addr, sizeof(server_addr));
            // assign IP, PORT
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
            server_addr.sin_port = htons(port);
            // connect the client socket to server socket
            if (::connect(client_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                throw SocketException(SocketException::SOCKET_CONNECT, errno);
            }
        }
       return Stream(client_socket_fd);
    }
}