//
// Created by marc on 11.11.22.
//
#include "socket_wrapper/Stream.h"
#include "socket_wrapper/SocketException.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <sys/poll.h>
#include <sys/eventfd.h>

#ifdef OPENSSL_FOUND
#include <openssl/ssl.h>
#endif


namespace socket_wrapper {
    Stream::Stream(int socket_fd) : stream_file_descriptor(socket_fd) {
        // create the end program listener
        stop_all_operations_event_fd.store(eventfd(0, EFD_SEMAPHORE));
        if (stop_all_operations_event_fd.load() == -1) {
            throw std::runtime_error("Failed to create event_fd");
        }
    }

    Stream &Stream::operator=(Stream &&stream_to_assign) noexcept {
        std::lock_guard<std::mutex> own_sock_lock_write(stream_file_descriptor_write_mtx);
        std::lock_guard<std::mutex> own_sock_lock_read(stream_file_descriptor_read_mtx);
        std::lock_guard<std::mutex> other_sock_lock_write(stream_to_assign.stream_file_descriptor_write_mtx);
        std::lock_guard<std::mutex> other_sock_lock_read(stream_to_assign.stream_file_descriptor_read_mtx);

        stream_file_descriptor = stream_to_assign.stream_file_descriptor;
        stream_to_assign.stream_file_descriptor = kInvalidSocketFdMarker;
        stop_all_operations_event_fd.store(stream_to_assign.stop_all_operations_event_fd.load());
#ifdef OPENSSL_FOUND
        ssl_data = stream_to_assign.ssl_data;
        stream_to_assign.ssl_data = nullptr;
#endif
        return *this;
    }


    Stream::Stream(Stream &&src) noexcept {
        {
            std::lock_guard<std::mutex> other_sock_lock_read(src.stream_file_descriptor_read_mtx);
            std::lock_guard<std::mutex> other_sock_lock_write(src.stream_file_descriptor_write_mtx);
            stream_file_descriptor = src.stream_file_descriptor;
            src.stream_file_descriptor = kInvalidSocketFdMarker;
#ifdef OPENSSL_FOUND
            ssl_data = src.ssl_data;
            src.ssl_data = nullptr;
            is_secure = src.is_secure;
#endif
        }
        stop_all_operations_event_fd.store(src.stop_all_operations_event_fd.load());
    }

    Stream::~Stream() noexcept {
        if (stream_file_descriptor < 0) {
            return;
        }
        // notify other functions (read) of termination
        uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
        ::write(stop_all_operations_event_fd, &semaphore_value, sizeof(semaphore_value));
        if (close(stream_file_descriptor)) {
            // TODO: log error
            std::terminate();
            // throw SocketException(SocketException::Type::SOCKET_CLOSE, errno); // terminates, most likely logic error
        }
#ifdef OPENSSL_FOUND
        if (is_secure) {
            SSL_free(ssl_data);
        }
#endif
    }

    size_t Stream::insecure_read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms) {
        size_t read_bytes = 0;
        while (read_bytes < min_bytes_to_read) {
            ssize_t read_result;
            {
                std::lock_guard<std::mutex> sock_lock(stream_file_descriptor_read_mtx);
                // use poll to find out if there is new data or a termination request
                std::array<pollfd, 2> poll_fds = {{{.fd = stream_file_descriptor, .events = POLLIN, .revents = 0},
                                                   {.fd = stop_all_operations_event_fd, .events = POLLIN, .revents = 0}}};
                if (poll(poll_fds.data(), poll_fds.size(), timeout_ms) == -1) {
                    // error while polling
                    throw SocketException(SocketException::SOCKET_POLL, errno);
                } else if (poll_fds[0].revents != 0) {
                    // we received data
                    read_result = ::read(stream_file_descriptor, (char *) buffer + read_bytes,
                                         max_bytes_to_read - read_bytes);
                    if (read_result == -1) {
                        throw SocketException(SocketException::SOCKET_READ, errno);
                    } else if (read_result == 0) {
                        // stream closed
                        throw SocketException(SocketException::SOCKET_CLOSED, 0);
                    }
                    read_bytes += read_result;
                } else if (poll_fds[1].revents != 0) {
                    // we received a termination request
                    throw SocketException(SocketException::SOCKET_TERMINATION_REQUEST, 0);
                } else {
                    // we received a timeout
                    throw SocketException(SocketException::SOCKET_READ_TIMEOUT, 0);
                }
            }
        }
        return read_bytes;
    }

    void Stream::insecure_write(const char *buffer, size_t size, int attempts) {
        ssize_t total_written_bytes = 0;
        while (attempts--) {
            ssize_t write_result;
            {
                std::lock_guard<std::mutex> sock_lock(stream_file_descriptor_write_mtx);
                write_result = ::write(stream_file_descriptor, buffer + total_written_bytes,
                                       size - total_written_bytes);
            }
            if (write_result == -1) {
                throw SocketException(SocketException::SOCKET_WRITE, errno);
            } else {
                total_written_bytes += write_result;
            }
            if (total_written_bytes == size) {
                break;
            }
            auto wait_time = kSocketRetryIntervallMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }
        if (total_written_bytes != size) {
            // we did not manage to write all the data
            throw SocketException(SocketException::SOCKET_WRITE_PARTIAL, errno, total_written_bytes);
        }
    }


    size_t Stream::secure_read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms) {
        size_t read_bytes = 0;
        while (read_bytes < min_bytes_to_read) {
            ssize_t read_result;
            {
                std::lock_guard<std::mutex> sock_lock(stream_file_descriptor_read_mtx);
                // use poll to find out if there is new data or a termination request
                std::array<pollfd, 2> poll_fds = {{{.fd = stream_file_descriptor, .events = POLLIN, .revents = 0},
                                                   {.fd = stop_all_operations_event_fd, .events = POLLIN, .revents = 0}}};
                if (poll(poll_fds.data(), poll_fds.size(), timeout_ms) == -1) {
                    // error while polling
                    throw SocketException(SocketException::SOCKET_POLL, errno);
                } else if (poll_fds[0].revents != 0) {
                    // we received data
                    read_result = ::SSL_read(ssl_data, (char *) buffer + read_bytes,
                                             max_bytes_to_read - read_bytes);
                    if (read_result == -1) {
                        throw SocketException(SocketException::SOCKET_READ, errno);
                    } else if (read_result == 0) {
                        // stream closed
                        throw SocketException(SocketException::SOCKET_CLOSED, 0);
                    }
                    read_bytes += read_result;
                } else if (poll_fds[1].revents != 0) {
                    // we received a termination request
                    throw SocketException(SocketException::SOCKET_TERMINATION_REQUEST, 0);
                } else {
                    // we received a timeout
                    throw SocketException(SocketException::SOCKET_READ_TIMEOUT, 0);
                }
            }
        }
        return read_bytes;
    }

    void Stream::secure_write(const char *buffer, size_t size, int attempts) {
        ssize_t total_written_bytes = 0;
        while (attempts--) {
            ssize_t write_result;
            {
                std::lock_guard<std::mutex> sock_lock(Stream::stream_file_descriptor_write_mtx);
                write_result = ::SSL_write(ssl_data, buffer + total_written_bytes,
                                           size - total_written_bytes);
            }
            if (write_result == -1) {
                throw SocketException(SocketException::SOCKET_WRITE, errno);
            } else {
                total_written_bytes += write_result;
            }
            if (total_written_bytes == size) {
                break;
            }
            auto wait_time = Stream::kSocketRetryIntervallMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }
        if (total_written_bytes != size) {
            // we did not manage to write all the data
            throw SocketException(SocketException::SOCKET_WRITE_PARTIAL, errno, total_written_bytes);
        }
    }

    void Stream::stopReads() {
        uint64_t semaphore_value = std::numeric_limits<uint16_t>::max();
        ::write(stop_all_operations_event_fd, &semaphore_value, sizeof(semaphore_value));
    }

    bool Stream::isSecure() const {
        return is_secure;
    }

    size_t Stream::read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms) {
        if (is_secure) {
            return secure_read(buffer, max_bytes_to_read, min_bytes_to_read, timeout_ms);
        } else {
            return insecure_read(buffer, max_bytes_to_read, min_bytes_to_read, timeout_ms);
        }
    }

    void Stream::write(const char *buffer, size_t size, int attempts) {
        if (is_secure) {
            secure_write(buffer, size, attempts);
        } else {
            insecure_write(buffer, size, attempts);
        }
    }

    Stream::Stream(int socket_fd, SSL *ssl_data) : stream_file_descriptor(socket_fd), ssl_data(ssl_data) {
        is_secure = true;
        /// create the end program listener
        stop_all_operations_event_fd.store(eventfd(0, EFD_SEMAPHORE));
        if (stop_all_operations_event_fd.load() == -1) {
            throw std::runtime_error("Failed to create event_fd");
        }
    }

    std::string Stream::get_chipher_name() {
#ifdef OPENSSL_FOUND
        if (is_secure) {
            return SSL_get_cipher_name(ssl_data);
        } else {
            return "None";
        }
#else
        return "OpenSSL not found, no encryption available";
#endif
    }
}