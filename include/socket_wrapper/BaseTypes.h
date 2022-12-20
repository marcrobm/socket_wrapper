#ifndef SOCKET_WRAPPER_BASETYPES_H
#define SOCKET_WRAPPER_BASETYPES_H

#include <map>
#include <string>
namespace socket_wrapper{

    struct NetworkInterface{
        std::string name;
        std::string ip_address;
        std::string netmask;
        std::string broadcast_address;
    };
    enum IP_VERSION{
        IPv4,
        IPv6
    };
    IP_VERSION IpVersionfromString(std::string v);
    extern std::map<IP_VERSION,std::string> getName;
    // some forward declarations
    class Stream;
    class StreamFactory;
    class ListenerBase;
    class ConditionalBufferedStream;
}

#endif //SOCKET_WRAPPER_BASETYPES_H
