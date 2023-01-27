#include <sys/poll.h>
#include <sys/eventfd.h>
#include <csignal>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include "socket_wrapper/UdpDatagram.h"
#include "unistd.h"
namespace socket_wrapper {

    UdpDatagram::UdpDatagram(const std::string &listener_ip_addr, uint16_t listener_port,
                             socket_wrapper::IP_VERSION version, int buffer_size) : ip_version(version) {
        if (version == IP_VERSION::IPv6) {
            throw std::logic_error("CreateDatagram does not support IPv6 as of now");
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

    std::vector<char> UdpDatagram::read(int timeout_ms) {
        std::array<pollfd, 2> poll_fds = {{{.fd = socket_fd, .events = POLLIN, .revents = 0},
                                           {.fd = stop_all_operations_event_fd, .events = POLLIN, .revents = 0}}};
        if (poll(poll_fds.data(), poll_fds.size(), timeout_ms) == timeout_ms) {
            throw socket_wrapper::SocketException(SocketException::SOCKET_POLL, errno);
        } else if (poll_fds[0].revents != 0) {
            struct msghdr msg;
            struct iovec iov;
            msg = setMsghdrParams(msg, iov);
            ssize_t read_result = recvmsg(socket_fd, &msg, 0);
            assertRecvmsgSucceded(msg, read_result);
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
            throw std::logic_error("IPv6 is not supported as of now");
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