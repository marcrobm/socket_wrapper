#ifndef SOCKET_WRAPPER_UTILS_H
#define SOCKET_WRAPPER_UTILS_H

#include "BaseTypes.h"

namespace socket_wrapper {
    std::vector<NetworkInterface> getLocalNetworkInterfaces(IP_VERSION version);
    NetworkInterface getInterfaceWithMostSpecificNetmask(IP_VERSION required_ip_version);
}

#endif //SOCKET_WRAPPER_UTILS_H
