#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/types.h>
#include <sys/poll.h>
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/SocketException.h"
#include "socket_wrapper/BaseTypes.h"
#include "socket_wrapper/Utils.h"

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
#ifdef OPENSSL_FOUND
    Stream StreamFactory::CreateSecureTcpStreamToServer(std::string ip_address, uint16_t port, IP_VERSION version) {
        int client_socket_fd;
        if (version == IP_VERSION::IPv6) {
            struct sockaddr_in6 server_addr;
            if ((client_socket_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            bzero(&server_addr, sizeof(server_addr));
            server_addr.sin6_family = AF_INET6;
            inet_pton(AF_INET6, ip_address.c_str(), &server_addr.sin6_addr);
            server_addr.sin6_port = htons(port);
            if (::connect(client_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                throw SocketException(SocketException::SOCKET_CONNECT, errno);
            }
        } else {
            struct sockaddr_in server_addr;
            if ((client_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            bzero(&server_addr, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
            server_addr.sin_port = htons(port);
            if (::connect(client_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                throw SocketException(SocketException::SOCKET_CONNECT, errno);
            }
        }
        // Connected to server

        socket_wrapper::ensureLibraryInitDone();

        SSL_CTX *ctx = SSL_CTX_new(TLS_method());
        ssl_st *ssl = SSL_new(ctx);
        if (!ssl) {
            throw SocketException(SocketException::SOCKET_SSL_CREATE, 0);
        }
        int ssl_sock = SSL_get_fd(ssl);
        SSL_set_fd(ssl, client_socket_fd);
        int err = SSL_connect(ssl);
        if (err <= 0) {
            throw SocketException(SocketException::SOCKET_SSL_CONNECT, err);
        }
        Stream x(client_socket_fd, ssl);
        return x;
    }
#endif
}
