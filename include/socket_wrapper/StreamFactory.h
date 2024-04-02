#ifndef EZNETWORK_STREAMFACTORY_H
#define EZNETWORK_STREAMFACTORY_H
#include <array>

#include "Stream.h"
#include "BaseTypes.h"

namespace socket_wrapper {
    class Stream;
    /**
     * @brief a class creating Streams
     */
    class StreamFactory {
    public:
        /**
         * Creates a Tcp Stream to a given Server
         * @param ip_address the ip address of the server to connect to
         * @param port the port to connect to
         * @param version the version of ip to use
         * @return a Stream connected to the server specified
         */
        static socket_wrapper::Stream CreateTcpStreamToServer(std::string ip_address, uint16_t port, IP_VERSION version = IPv4);

        static std::array<Stream, 2> CreatePipe();

        static Stream CreateSecureTcpStreamToServer(std::string ip_address, uint16_t port, IP_VERSION version);
    };
}
#endif //EZNETWORK_STREAMFACTORY_H
