#include <sys/poll.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include "socket_wrapper/Stream.h"
#include "socket_wrapper/SocketException.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include "socket_wrapper/UdpDatagram.h"

namespace socket_wrapper {

    UdpDatagram::UdpDatagram(const std::string &listener_ip_addr, uint16_t listener_port,
                             socket_wrapper::IP_VERSION version, int buffer_size) : ip_version(version) {
        if (version == IP_VERSION::IPv6) {
            struct sockaddr_in6 receiver_address;
            if ((socket_fd = socket(AF_INET6, SOCK_DGRAM | SO_REUSEADDR, IPPROTO_UDP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            int one = 1;
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
#ifdef SO_REUSEPORT
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(int));
#endif
            bzero(&receiver_address, sizeof(receiver_address));
            // assign IP, PORT
            receiver_address.sin6_family = AF_INET6;
            receiver_address.sin6_addr = in6addr_any;
            receiver_address.sin6_port = htons(listener_port);
            if (::bind(socket_fd, (struct sockaddr *) &receiver_address, sizeof(receiver_address)) < 0) {
                throw SocketException(SocketException::SOCKET_BIND, errno);
            }
        } else {
            struct sockaddr_in receiver_address;
            if ((socket_fd = socket(AF_INET, SOCK_DGRAM | SO_REUSEADDR, IPPROTO_UDP)) < 0) {
                throw SocketException(SocketException::SOCKET_SOCKET, errno);
            }
            int one = 1;
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
#ifdef SO_REUSEPORT
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(int));
#endif
            bzero(&receiver_address, sizeof(receiver_address));
            // assign IP, PORT
            receiver_address.sin_family = AF_INET;
            receiver_address.sin_addr.s_addr = inet_addr(listener_ip_addr.c_str());
            receiver_address.sin_port = htons(listener_port);
            // connect the client socket to server socket
            if (::bind(socket_fd, (struct sockaddr *) &receiver_address, sizeof(receiver_address)) < 0) {
                throw SocketException(SocketException::SOCKET_BIND, errno);
            }

        }
        buffer = std::vector<char>(buffer_size);
        stop_all_operations_event_fd.store(eventfd(0, EFD_SEMAPHORE));
        if (stop_all_operations_event_fd.load() == -1) {
            throw std::runtime_error("Failed to create event_fd");
        }
    }

    UdpDatagram &UdpDatagram::operator=(UdpDatagram &&stream_to_assign) noexcept {
        socket_fd = stream_to_assign.socket_fd.load();
        stream_to_assign.socket_fd = kInvalidSocketFdMarker;
        stop_all_operations_event_fd.store(stream_to_assign.stop_all_operations_event_fd.load());
        return *this;
    }


    UdpDatagram::UdpDatagram(UdpDatagram &&src) noexcept {
        {
            socket_fd = src.socket_fd.load();
            src.socket_fd = kInvalidSocketFdMarker;
        }
        stop_all_operations_event_fd.store(src.stop_all_operations_event_fd.load());
    }

    UdpDatagram::~UdpDatagram() noexcept {
        if (socket_fd < 0) {
            return;
        }
        // notify other functions (read) of termination
        uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
        ::write(stop_all_operations_event_fd, &semaphore_value, sizeof(semaphore_value));
        if (close(socket_fd)) {
            // TODO: log error
            std::terminate();
            // throw SocketException(SocketException::Type::SOCKET_CLOSE, errno); // terminates, most likely logic error
        }
    }

    std::vector<char> UdpDatagram::read(int timeout_ms,std::string* sender_ip) {
        std::array<pollfd, 2> poll_fds = {{{.fd = socket_fd, .events = POLLIN, .revents = 0},
                                           {.fd = stop_all_operations_event_fd, .events = POLLIN, .revents = 0}}};
        if (poll(poll_fds.data(), poll_fds.size(), timeout_ms) == timeout_ms) {
            throw socket_wrapper::SocketException(SocketException::SOCKET_POLL, errno);
        } else if (poll_fds[0].revents != 0) {
            struct msghdr msg;
            struct iovec iov;
            msg = setMsghdrParams(msg, iov);
            struct sockaddr_storage src_addr_storage;
            msg.msg_name = &src_addr_storage;
            msg.msg_namelen = sizeof(src_addr_storage);
            ssize_t read_result = recvmsg(socket_fd, &msg, 0);
            assertRecvmsgSucceded(msg, read_result);
            std::array<char, 128> addr_buffer{};
            memset(addr_buffer.data(), 0, addr_buffer.size());
            if (ip_version == IP_VERSION::IPv6) {
                struct sockaddr_in6 *src_addr = (struct sockaddr_in6 *) msg.msg_name;
                inet_ntop(AF_INET6, &src_addr->sin6_addr, addr_buffer.data(), addr_buffer.size());
            } else {
                struct sockaddr_in *src_addr = (struct sockaddr_in *) msg.msg_name;
                inet_ntop(AF_INET, &src_addr->sin_addr, addr_buffer.data(), addr_buffer.size());
            }
            if(sender_ip){
                *sender_ip = std::string(addr_buffer.data());
            }
            return {buffer.begin(), buffer.begin() + read_result};
        } else if (poll_fds[1].revents != 0) {
            throw SocketException(SocketException::SOCKET_TERMINATION_REQUEST, errno);
        } else {
            throw SocketException(SocketException::SOCKET_READ_TIMEOUT, errno);
        }
    }

    void UdpDatagram::assertRecvmsgSucceded(const msghdr &msg, ssize_t read_result) const {
        if (read_result == -1) {
            throw SocketException(SocketException::SOCKET_READ, errno);
        } else if (msg.msg_flags & MSG_TRUNC) {
            throw SocketException(SocketException::SOCKET_RECEIVE_BUFFER_TOO_SMALL, errno);
        }
    }

    msghdr &UdpDatagram::setMsghdrParams(msghdr &msg, iovec &iov) {
        iov = {buffer.data(), buffer.size()};
        msg.msg_iov = &iov;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        return msg;
    }


    void UdpDatagram::write(const std::vector<char> &msg_data, const std::string &destination_ip, int port) {
        if (ip_version == IP_VERSION::IPv6) {
            struct sockaddr_in6 dst_addr;
            // assign IP, PORT
            dst_addr.sin6_family = AF_INET6;
            dst_addr.sin6_port = htons(port);
            if (inet_pton(AF_INET6, destination_ip.c_str(), &dst_addr.sin6_addr) != 1) {
                 throw SocketException(SocketException::SOCKET_WRITE, errno);
            }
            if (sendto(socket_fd, msg_data.data(), msg_data.size(), 0, (struct sockaddr *) &dst_addr,
                       sizeof(dst_addr)) == -1) {
                throw SocketException(SocketException::SOCKET_WRITE, errno);
            }
        } else {
            struct sockaddr_in dst_addr;
            // assign IP, PORT
            dst_addr.sin_family = AF_INET;
            dst_addr.sin_addr.s_addr = inet_addr(destination_ip.c_str());
            dst_addr.sin_port = htons(port);
            if (sendto(socket_fd, msg_data.data(), msg_data.size(), 0, (struct sockaddr *) &dst_addr,
                       sizeof(dst_addr)) == -1) {
                throw SocketException(SocketException::SOCKET_WRITE, errno);
            }
        }
    }
    void UdpDatagram::stopReads() {
        uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
        ::write(stop_all_operations_event_fd, &semaphore_value, sizeof(semaphore_value));
    }

    void UdpDatagram::subscribeToMulticast(const std::string &group_addr) {
        // use setsockopt() to join a multicast group
        uint32_t addr_prefix = ntohl(inet_addr(group_addr.c_str()));
        if( addr_prefix >= ntohl(inet_addr("224.0.0.0")) && addr_prefix < ntohl(inet_addr("240.0.0.0"))) {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(group_addr.c_str());
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);            int one = 1;
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0) {
                throw SocketException(SocketException::SOCKET_BIND, errno);
            }
        }else{
            throw SocketException(SocketException::SOCKET_JOIN_MULTICAST, errno);
        }
    }
}
