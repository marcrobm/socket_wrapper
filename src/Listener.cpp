#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <fcntl.h>
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
        // create the socket, we do not want it to block as we use poll
        listener_socket_fd.store(socket(ip_v, SOCK_STREAM, IPPROTO_TCP));
        if (listener_socket_fd.load() < 0) {
            throw SocketException(SocketException::SOCKET_SOCKET, errno);
        }
        // make non blocking
        if (fcntl(listener_socket_fd.load(), F_SETFL, O_NONBLOCK) < 0) {
            throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
        }
        // assign ip and port
        if (version == IPv6) {
            struct sockaddr_in6 servaddr, cli;
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin6_family = ip_v;
            servaddr.sin6_addr = in6addr_any;
            servaddr.sin6_port = htons(port);
            int optval = 1;
            if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
            }
#ifdef SO_REUSEPORT
            if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
                throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
            }
#endif

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
            int optval = 1;
            if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
            }
#ifdef SO_REUSEPORT
            if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
                throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
            }
#endif
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

    Stream Listener::accept(int timeout) {

        std::array<pollfd, 2> poll_fds = {{{.fd = listener_socket_fd.load(), .events = POLLIN, .revents = 0},
                                           {.fd = listener_end_fd, .events = POLLIN, .revents = 0}}};
        if (poll(poll_fds.data(), poll_fds.size(), timeout) == -1) {
            // error while polling
            throw SocketException(SocketException::SOCKET_POLL, errno);
        } else if (poll_fds[0].revents != 0) {
            // new client
            int connecting_fd;
            struct sockaddr incoming_stream_addr;
            socklen_t incoming_stream_addr_length = sizeof(sockaddr);
            if ((connecting_fd = ::accept(listener_socket_fd.load(), &incoming_stream_addr,
                                          &incoming_stream_addr_length)) < 0) {
                throw SocketException(SocketException::SOCKET_ACCEPT, errno);
            } else {
#ifdef OPENSSL_FOUND
                if (ssl_ctx == nullptr) {
                    return Stream(connecting_fd);
                } else {
                    auto ssl = SSL_new(ssl_ctx);
                    SSL_set_fd(ssl, connecting_fd);
                    // within timeout, check if we can accept the connection
                    auto start_time = std::chrono::steady_clock::now();
                    bool accepted = false;
                    while (std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time).count() < timeout || timeout < 0) {
                        if (SSL_accept(ssl) > 0) {
                            accepted = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                    if (!accepted) {
                        throw SocketException(SocketException::SOCKET_ACCEPT, errno);
                    }
                    return Stream(connecting_fd, ssl);
                }
#else
                return Stream(connecting_fd);
#endif

            }
        } else if (poll_fds[1].revents != 0) {
            throw SocketException(SocketException::SOCKET_TERMINATION_REQUEST, errno);
        }
        throw SocketException(SocketException::SOCKET_ACCEPT, errno);
    }

    Listener::Listener(int
                       port, IP_VERSION
                       version, bool
                       reuse) {
        createSocket(port, version, reuse);
    }

    void Listener::createSocket(int port, const IP_VERSION &version, bool reuse) {
        auto ip_v = (version == IPv6) ? AF_INET6 : AF_INET;
        listener_end_fd.store(eventfd(0, EFD_SEMAPHORE));
        stopped_accepting.store(false);
        // create the socket, we do not want it to block as we use poll
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
            // reuse addr
            if (reuse) {
                int optval = 1;
                if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                    throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
                }
#ifdef SO_REUSEPORT
                if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
                    throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
                }
#endif
            }

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
            if (reuse) {
                int optval = 1;
                if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                    throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
                }
#ifdef SO_REUSEPORT
                if (setsockopt(listener_socket_fd.load(), SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
                    throw SocketException(SocketException::SOCKET_SET_OPTION, errno);
                }
#endif
            }
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

    void Listener::stopAccepting() {
        if (!stopped_accepting.load()) {
            uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
            write(listener_end_fd, &semaphore_value, sizeof(semaphore_value));
            ::close(listener_socket_fd.load());
            stopped_accepting.store(true);
        }
    }

    Listener::~Listener()
    noexcept {
        stopAccepting();
    }

    int Listener::getFdForPoll() {
        return listener_socket_fd;
    }

#ifdef OPENSSL_FOUND

    Listener::Listener(std::string
                       cert_path, std::string
                       key_path, int
                       port, IP_VERSION
                       version, bool
                       reuse) {
        const SSL_METHOD *method;
        method = TLS_server_method();
        ssl_ctx = SSL_CTX_new(method);
        if (!ssl_ctx) {
            throw SocketException(SocketException::SOCKET_SSL_CREATE, 0);
        }
        if (SSL_CTX_use_certificate_file(ssl_ctx, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw SocketException(SocketException::SOCKET_SSL_CERTIFICATE, 0);
        }
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            throw SocketException(SocketException::SOCKET_SSL_KEY, 0);
        }
        createSocket(port, version, reuse);


    }

#endif
}