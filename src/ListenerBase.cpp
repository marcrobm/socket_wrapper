#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include "socket_wrapper/Listener.h"
#include "socket_wrapper/SocketException.h"
#include "unistd.h"

namespace socket_wrapper {

    ListenerBase::ListenerBase(int port, IP_VERSION version) : stopped_accepting{false} {
        auto ip_v = (version == IP_VERSION::IPv6) ? AF_INET6 : AF_INET;
        // create the end programm listener
        listener_end_fd.store(eventfd(0, EFD_SEMAPHORE));
        if (listener_end_fd.load() == -1) {
            throw std::runtime_error("Failed to create event_fd");
        }
        // create the socket, we do not want it to block as we use poll TODO: currently blocking, does poll'ing itself suffice?
        listener_socket_fd.store(socket(ip_v, SOCK_STREAM, IPPROTO_TCP));
        if (listener_socket_fd.load() < 0) {
            throw SocketException(SocketException::SOCKET_SOCKET, errno);
        }
        // assign ip and port
        if (version == IPv6) {
            struct sockaddr_in6 servaddr, cli;
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin6_family = ip_v;
            servaddr.sin6_addr = in6addr_any;
            servaddr.sin6_port = htons(port);
            // bind socket to address
            if ((bind(listener_socket_fd.load(), (sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
                throw SocketException(SocketException::SOCKET_BIND, errno);
            }
        } else {
            struct sockaddr_in servaddr, cli;
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin_family = ip_v;
            servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
            servaddr.sin_port = htons(port);
            // bind socket to address
            if ((bind(listener_socket_fd.load(), (sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
                throw SocketException(SocketException::SOCKET_BIND, errno);
            }
        }

        // listen for connection requests
        if ((listen(listener_socket_fd.load(), 5)) != 0) {
            throw SocketException(SocketException::SOCKET_LISTEN, errno);
        }
    }

    void ListenerBase::startAccepting() {
        // handle incoming clients
        handle_incoming_streams_task = std::move(std::thread([&]() {
            handleIncomingStreams();
        }));
    }

    void ListenerBase::handleIncomingStreams() {
        try {
            while (true) {
                // use poll to find out if there are any waiting clients
                std::array<pollfd, 2> poll_fds = {{{.fd = listener_socket_fd.load(), .events = POLLIN, .revents = 0},
                                                   {.fd = listener_end_fd, .events = POLLIN, .revents = 0}}};
                if (poll(poll_fds.data(), poll_fds.size(), -1) == -1) {
                    // error while polling
                    continue;
                } else if (poll_fds[0].revents != 0) {
                    // new client
                    int connecting_fd;
                    struct sockaddr incoming_stream_addr;
                    socklen_t incoming_stream_addr_length = sizeof(sockaddr);
                    if ((connecting_fd = accept(listener_socket_fd.load(), &incoming_stream_addr,
                                                &incoming_stream_addr_length)) < 0) {
                        // failed to accept a client should be ignored, as it is not an error within the ecs, TODO: only log it
                    } else {
                        // accepted new client
                        try {
                            onIncomingStream(Stream(connecting_fd));
                        } catch (const std::exception &e) {
                            std::cout << "Listener: exception onIncomingStream " << +e.what() << " was thrown "
                                      << std::endl;
                        }
                    }
                } else if (poll_fds[1].revents != 0) {
                    return; // Listener termination request
                }
            }
        } catch (std::exception ex) {
            std::cout << "Listener: exception " << +ex.what() << " was thrown " << std::endl;
        }
        std::cout << "exited handler" << std::endl;
    }

    ListenerBase::~ListenerBase() noexcept {
        // std::cout << "stopping to listen" << std::endl;
        stopAccepting();
        // std::cout << "closed listener socket, destructor end" << std::endl;
    }

    void ListenerBase::stopAccepting() {
        if (!stopped_accepting.load()) {
            uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
            write(listener_end_fd, &semaphore_value, sizeof(semaphore_value));
            if (handle_incoming_streams_task.joinable()) {
                handle_incoming_streams_task.join();
            }
            ::close(listener_socket_fd.load());

            stopped_accepting.store(true);
        }
    }
}