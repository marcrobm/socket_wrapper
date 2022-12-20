#include <vector>
#include <mutex>
#include <atomic>
#include <sys/socket.h>

#include "socket_wrapper/BaseTypes.h"
#include "socket_wrapper/SocketException.h"
#include "socket_wrapper/BaseTypes.h"

namespace socket_wrapper {
/**
 * @brief A wrapper class for sockets of Type SOCK_DGRAM
 */
    class UdpDatagram {
    public:
        UdpDatagram() = delete;
        UdpDatagram(const std::string &listener_ip_addr, uint16_t listener_port, IP_VERSION version, int buffer_size = 65536);

        std::vector<char> read(int timeout_ms = -1);
        void write(const std::vector<char> &msg_data, const std::string &destination_ip, int port);

        // moving is allowed
        UdpDatagram &operator=(UdpDatagram &&stream_to_assign) noexcept ;
        UdpDatagram(UdpDatagram const &) = delete;
        UdpDatagram(UdpDatagram &&src) noexcept;
        ~UdpDatagram() noexcept;
    private:
        explicit UdpDatagram(int socket_fd, int buffer_size = 65536);
        std::vector<char> buffer;
        int socket_fd;
        IP_VERSION ip_version;
        std::atomic<int> stop_all_operations_event_fd{};
        std::recursive_mutex socket_mutex;
        msghdr &setMsghdrParams(msghdr &msg, iovec &iov);
        static int const kInvalidSocketFdMarker = -1;
        void assertRecvmsgSucceded(const msghdr &msg, ssize_t read_result) const;

    };
}