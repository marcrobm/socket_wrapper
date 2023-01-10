//
// Created by marc on 11.11.22.
//
#include <cstring>
#include "socket_wrapper/SocketException.h"

namespace socket_wrapper {
    const std::map<SocketException::Type, std::string> SocketException::SocketExceptionTypeName = {
            {SocketException::Type::SOCKET_SOCKET,                   "SOCKET_SOCKET"},
            {SocketException::Type::SOCKET_BIND,                     "SOCKET_BIND"},
            {SocketException::Type::SOCKET_LISTEN,                   "SOCKET_LISTEN"},
            {SocketException::Type::SOCKET_ACCEPT,                   "SOCKET_ACCEPT"},
            {SocketException::Type::SOCKET_WRITE,                    "SOCKET_WRITE"},
            {SocketException::Type::SOCKET_WRITE_PARTIAL,            "SOCKET_WRITE_PARTIAL"},
            {SocketException::Type::SOCKET_READ,                     "SOCKET_READ"},
            {SocketException::Type::SOCKET_CLOSE,                    "SOCKET_CLOSE"},
            {SocketException::Type::SOCKET_CONNECT,                  "SOCKET_CONNECT"},
            {SocketException::Type::SOCKET_PAIR,                     "SOCKET_PAIR"},
            {SocketException::Type::SOCKET_TERMINATION_REQUEST,      "SOCKET_TERMINATION_REQUEST"},
            {SocketException::Type::SOCKET_CLOSED,                   "SOCKET_CLOSED"},
            {SocketException::Type::SOCKET_READ_TIMEOUT,             "SOCKET_READ_TIMEOUT"},
            {SocketException::Type::SOCKET_POLL,                     "SOCKET_POLL"},
            {SocketException::Type::SOCKET_RECEIVE_BUFFER_TOO_SMALL, "SOCKET_RECEIVE_BUFFER_TOO_SMALL"},
            {SocketException::Type::SOCKET_JOIN_MULTICAST,           "SOCKET_JOIN_MULTICAST"}
    };

    const char *SocketException::what() const noexcept {
        description = SocketExceptionTypeName.at(exception_type) + ":" + std::strerror(c_error);
        return description.c_str();
    }

    SocketException::SocketException(SocketException::Type t, int c_error, ssize_t processed_bytes) : exception_type(t),
                                                                                                      c_error(c_error),
                                                                                                      processed_bytes(
                                                                                                              processed_bytes) {}
}