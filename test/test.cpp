#include "test.h"
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/UdpDatagram.h"
#include "socket_wrapper/BaseTypes.h"
#include "socket_wrapper/Utils.h"

using namespace std;
#define TEST_IP_VERSION socket_wrapper::IPv4
//#define TEST_IP_VERSION socket_wrapper::IPv6

TEST(ConditionalBufferedStream, ReadUntilDelimiter) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(BufferedStream(std::move(streams[1]), 512));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();

    streams[0].write("xyz", 3, 1);
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
    streams[0].write("def", 3, 1);
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
    streams[0].write("\na\n", 3, 1);
    ASSERT_POLL_GOT_EVENT(on_newline_fd)
    ASSERT_READ_EQ(cstream, on_newline_fd, "xyzdef\n");
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_EQ(cstream, on_newline_fd, "a\n");
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
    streams[0].write("123", 3, 1);
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
    streams[0].write("a\nc", 3, 1);
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_EQ(cstream, on_newline_fd, "123a\n");
}

TEST(ConditionalBufferedStream, ReadBlocking) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(BufferedStream(std::move(streams[1]), 512));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();

    streams[0].write("abc\nxyz\n", 8, 1);
    ASSERT_READ_BLOCKING_EQ(cstream, on_newline_fd, "abc\n");
    ASSERT_READ_BLOCKING_EQ(cstream, on_newline_fd, "xyz\n");
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
}
TEST(ConditionalBufferedStream, ReadNonBlocking) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();

    streams[0].write("abc\nxyz\n", 8, 1);
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, on_newline_fd, "abc\n");
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, on_newline_fd, "xyz\n");
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
}


TEST(Datagram, SendthenRead) {
    auto conn = socket_wrapper::UdpDatagram("127.0.0.0", 8001, TEST_IP_VERSION);
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    conn.write(sent_packet, "127.0.0.0", 8001);
    auto received_packet = conn.read(100);
    ASSERT_EQ(received_packet, sent_packet);
}
TEST(Datagram, Multicast) {
    auto conn = socket_wrapper::UdpDatagram("0.0.0.0", 8001, TEST_IP_VERSION);
    conn.subscribeToMulticast("224.1.2.3");
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    conn.write(sent_packet, "224.1.2.3", 8001);
    auto received_packet = conn.read(100);
    ASSERT_EQ(received_packet, sent_packet);
}
TEST(Utils, GetLocalIpAddresses) {
    auto interfaces = socket_wrapper::getLocalNetworkInterfaces(socket_wrapper::IPv4);
    auto interfacesV6 = socket_wrapper::getLocalNetworkInterfaces(socket_wrapper::IPv6);
    interfaces.insert(interfaces.end(), interfacesV6.begin(), interfacesV6.end());
    ASSERT_FALSE(interfaces.empty());
    for (auto &interface: interfaces) {
        cout << "name:" << interface.name
             << " , ip:" << interface.ip_address
             << " , netmask:" << interface.netmask
             << " , broadcast:" << interface.broadcast_address << endl;
        ASSERT_FALSE(interface.ip_address.empty());
    }
    cout << "most likely primary IPv4 ip:" + getInterfaceWithMostSpecificNetmask(socket_wrapper::IPv4).ip_address << endl;
    cout << "most likely primary IPv6 ip:" + getInterfaceWithMostSpecificNetmask(socket_wrapper::IPv6).ip_address << endl;
}