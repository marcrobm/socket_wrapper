#ifndef EZNETWORK_SOCKETEXCEPTION_H
#define EZNETWORK_SOCKETEXCEPTION_H

#include <cstdint>
#include <string>
#include <map>

namespace socket_wrapper {
    class SocketException : public std::exception {
    public:
        enum Type : uint8_t {
            SOCKET_SOCKET,
            SOCKET_BIND,
            SOCKET_LISTEN,
            SOCKET_ACCEPT,
            SOCKET_WRITE,
            SOCKET_WRITE_PARTIAL,
            SOCKET_READ,
            SOCKET_CLOSE,
            SOCKET_CONNECT,
            SOCKET_PAIR,
            SOCKET_TERMINATION_REQUEST,
            SOCKET_CLOSED,
            SOCKET_READ_TIMEOUT,
        };

        explicit SocketException(Type t, int c_error = -1, ssize_t processed_bytes = -1);
        const char *what() const noexcept override;
        const ssize_t processed_bytes = 0;
        const static std::map<Type, std::string> SocketExceptionTypeName;
        const int c_error;
        const Type exception_type;
    private:
        mutable std::string description;
    };
}
#endif //EZNETWORK_SOCKETEXCEPTION_H