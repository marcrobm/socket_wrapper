#include <string>
#include <ifaddrs.h>
#include <array>
#include <arpa/inet.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <regex>
#include "socket_wrapper/Utils.h"

namespace socket_wrapper {
    // declared here to not make them "public" in header
    std::basic_string<char> getIpV6AddressOf(struct sockaddr *ifAddressesIterator);

    std::string getIpV4AddressOf(struct sockaddr *ifAddressesIterator);

    std::string getIpAddressOf(struct sockaddr *ifAddressesIterator);

    std::vector<NetworkInterface> getLocalNetworkInterfaces(IP_VERSION version) {
        std::vector<NetworkInterface> found_addresses;
        struct ifaddrs *ifAddresses;
        getifaddrs(&ifAddresses);
        struct ifaddrs *ifAddressesIterator = ifAddresses;
        while (ifAddressesIterator != nullptr) {
            if (ifAddressesIterator->ifa_addr && ifAddressesIterator->ifa_addr->sa_family == (version==IPv4?AF_INET:AF_INET6)) {
                NetworkInterface found_interface = {
                        .name = ifAddressesIterator->ifa_name,
                        .ip_address = getIpAddressOf(ifAddressesIterator->ifa_addr),
                        .netmask = getIpAddressOf(ifAddressesIterator->ifa_netmask),
                        .broadcast_address =  getIpAddressOf(ifAddressesIterator->ifa_broadaddr)
                };
                found_addresses.push_back(found_interface);
            }
            ifAddressesIterator = ifAddressesIterator->ifa_next;
        }
        return found_addresses;
    }

    NetworkInterface getInterfaceWithMostSpecificNetmask(IP_VERSION required_ip_version){
        auto interfaces = getLocalNetworkInterfaces(required_ip_version);
        std::regex invalid_chars(":|.|0");
        std::sort(interfaces.begin(), interfaces.end(), [&](const NetworkInterface& a, const NetworkInterface& b){
            // hacky way to roughly compare netmasks
            auto a_netmask = std::regex_replace(a.netmask, invalid_chars, "x");
            auto b_netmask = std::regex_replace(b.netmask, invalid_chars, "x");
            // ordering: ip should not be localhost, then prefer more specific netmasks
            return b.ip_address == "::1" || b.ip_address == "127.0.0.1" || (a_netmask.length() > b_netmask.length());
        });
        return interfaces.front();
    }

    std::string getIpAddressOf(struct sockaddr *ifAddressesIterator) {
        if (ifAddressesIterator) {
            if (ifAddressesIterator->sa_family == AF_INET6) {
                return getIpV6AddressOf(ifAddressesIterator);
            } else if (ifAddressesIterator->sa_family == AF_INET) {
                return getIpV4AddressOf(ifAddressesIterator);
            } else {
                throw std::runtime_error("Unknown address family:" + std::to_string(ifAddressesIterator->sa_family));
            }
        }
        return "";
    }

    std::string getIpV6AddressOf(struct sockaddr *ifAddressesIterator) {
        std::array<char, INET6_ADDRSTRLEN> ip_address_buffer{};
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *) ifAddressesIterator)->sin6_addr,
                  ip_address_buffer.data(), INET6_ADDRSTRLEN);
        auto address = std::string(ip_address_buffer.data());
        return address;
    }

    std::string getIpV4AddressOf(struct sockaddr *ifAddressesIterator) {
        std::array<char, INET_ADDRSTRLEN> ip_address_buffer{};
        inet_ntop(AF_INET, &((struct sockaddr_in *) ifAddressesIterator)->sin_addr,
                  ip_address_buffer.data(), INET_ADDRSTRLEN);
        auto address = std::string(ip_address_buffer.data());
        return address;
        return "";
    }
}