#ifndef EZNETWORK_BASETYPES_H
#define EZNETWORK_BASETYPES_H

#include <map>
#include <string>
namespace socket_wrapper{
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

#endif //EZNETWORK_BASETYPES_H
