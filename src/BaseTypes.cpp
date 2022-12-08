#include <stdexcept>
#include "socket_wrapper/BaseTypes.h"

std::map<socket_wrapper::IP_VERSION, std::string> socket_wrapper::getName = {
        {IPv6, "IPv6"},
        {IPv4, "IPv4"}
};

socket_wrapper::IP_VERSION socket_wrapper::IpVersionfromString(std::string v) {
    if (v == "IPv4") {
        return socket_wrapper::IPv4;
    } else if (v == "IPv6") {
        return socket_wrapper::IPv6;
    } else {
        throw std::invalid_argument("only supported ip versions are IPv6 and IPv4, not [" + v + "]");
    }
}
