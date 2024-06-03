#include "test.h"
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/UdpDatagram.h"
#include "socket_wrapper/BaseTypes.h"
#include "socket_wrapper/Utils.h"

using namespace std;
//#define TEST_IP_VERSION socket_wrapper::IPv4
#define TEST_IP_VERSION socket_wrapper::IPv6
//#define TEST_LOCAL_ADDRESS "127.0.0.0"
#define TEST_LOCAL_ADDRESS "::1"


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
TEST(ConditionalBufferedStream, ReadNonBlockingStr) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();

    streams[0].write("abc\nxyz\n", 8, 1);
    ASSERT_READ_BLOCKING_STR_EQ(cstream, on_newline_fd, "abc\n");
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, on_newline_fd, "xyz\n");
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
}
TEST(ConditionalBufferedStream, DiscardData) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));
    // In a stream like asdaA123cvBasd we want to always capture stuff between A and B
    int fancy_fd = cstream->createEventfdOnCondition([](const std::vector<char>& data){
        auto end_marker = std::find(data.begin(), data.end(), 'Z');
        if(end_marker == data.end()){
            return 0;//not enough data
        }
        if(data.at(0) == 'A'){
            return (int)std::distance(std::begin(data),end_marker);// take everything till B
        }else{
            //discard everything till next A
            auto next_start_marker = std::find(data.begin(), data.end(), 'A');
            if(next_start_marker == data.end()){
                return (int)(-data.size());//discard everything
            }else{
                return (int)(-std::distance(std::begin(data),next_start_marker));
            }
        }
    });
    cstream->start();

    streams[0].write("abc\nAyzxZbdfAAZ\n", 16, 1);
    ASSERT_READ_BLOCKING_STR_EQ(cstream, fancy_fd, "Ayzx");
    ASSERT_POLL_GOT_EVENT(fancy_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, fancy_fd, "AA");
    ASSERT_POLL_TIMED_OUT(fancy_fd);
}
TEST(ConditionalBufferedStream, ReadNonBlockingTimeout) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));
    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();
    ASSERT_ANY_THROW(cstream->readBlocking(on_newline_fd, 1));
}

TEST(ConditionalBufferedStream, ThrowExceptionIfClosed) {
    using namespace socket_wrapper;
    std::shared_ptr<ConditionalBufferedStream> cstream;
    std::shared_ptr<Stream> istream;
    {
        auto streams = StreamFactory::CreatePipe();
        cstream = std::make_shared<ConditionalBufferedStream>(
                std::move(BufferedStream(std::move(streams[1]), 512)));
        istream = std::make_shared<Stream>(std::move(streams[0]));
    }

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();
    istream->write("abc\n", 8, 1);
    ASSERT_READ_BLOCKING_STR_EQ(cstream, on_newline_fd, "abc\n");
    istream = nullptr;// delete stream to close it
    ASSERT_ANY_THROW(cstream->readBlocking(on_newline_fd, 1));
}


TEST(ConditionalBufferedStream, ReadMixtureOfTypes) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();

    streams[0].write("abc\nxyz\n", 8, 1);
    streams[0].write("abc\nxyz\n", 8, 1);
    ASSERT_READ_BLOCKING_STR_EQ(cstream, on_newline_fd, "abc\n");
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, on_newline_fd, "xyz\n");
    ASSERT_READ_BLOCKING_STR_EQ(cstream, on_newline_fd, "abc\n");
    ASSERT_POLL_GOT_EVENT(on_newline_fd);
    ASSERT_READ_NON_BLOCKING_EQ(cstream, on_newline_fd, "xyz\n");
    ASSERT_POLL_TIMED_OUT(on_newline_fd);
}

TEST(ConditionalBufferedStream, AbortReads) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto cstream = std::make_shared<ConditionalBufferedStream>(std::move(BufferedStream(std::move(streams[1]), 512)));

    auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
    int on_newline_fd = cstream->createEventfdOnCondition(newline_condition);
    cstream->start();
    auto t = std::thread([&](){
       try{
           cstream->readBlocking(on_newline_fd, 1000);
           FAIL() << "readBlocking should have thrown exception";
       }catch (SocketException& e){
          ASSERT_EQ(SocketException::SocketExceptionTypeName.at(e.exception_type),
                    SocketException::SocketExceptionTypeName.at(SocketException::Type::SOCKET_TERMINATION_REQUEST));
       }
    });
    this_thread::sleep_for(500ms);
    cstream->stopReads();
    t.join();
}


TEST(Listener, ConnectDirectly){
    using namespace socket_wrapper;
    Listener A = Listener(8234,TEST_IP_VERSION);
    auto connecting_client = std::jthread([&](){
        auto stream = StreamFactory::CreateTcpStreamToServer("127.0.0.1", 8234, TEST_IP_VERSION);
        stream.write("abc", 3, 1);
        this_thread::sleep_for(500ms);
    });
    auto new_client_connection = A.accept(500);
    std::vector<char> buffer(64);
    new_client_connection.read(buffer.data(), 3, 1);
    ASSERT_EQ(buffer[0], 'a');
}


TEST(Datagram, SendthenRead) {
    auto conn = socket_wrapper::UdpDatagram(TEST_LOCAL_ADDRESS, 8001, TEST_IP_VERSION);
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    conn.write(sent_packet, TEST_LOCAL_ADDRESS, 8001);
    auto received_packet = conn.read(100);
    ASSERT_EQ(received_packet, sent_packet);
}
TEST(Datagram, MulticastIpV4) {
    auto conn = socket_wrapper::UdpDatagram("0.0.0.0", 8001, socket_wrapper::IPv4);
    conn.subscribeToMulticast("224.1.2.3");
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    conn.write(sent_packet, "224.1.2.3", 8001);
    auto received_packet = conn.read(100);
    ASSERT_EQ(received_packet, sent_packet);
}

TEST(Datagram, MultipleMulticast) {
    auto connA = socket_wrapper::UdpDatagram("0.0.0.0", 8001, socket_wrapper::IPv4);
    auto connB = socket_wrapper::UdpDatagram("0.0.0.0", 8001, socket_wrapper::IPv4);
    connA.subscribeToMulticast("224.1.2.3");
    connB.subscribeToMulticast("224.1.2.3");
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    connA.write(sent_packet, "224.1.2.3", 8001);

    auto received_packet = connB.read(100);
    ASSERT_EQ(received_packet, sent_packet);
}

TEST(Datagram, GetSenderAddr) {
    auto connA = socket_wrapper::UdpDatagram("::1", 8001, socket_wrapper::IPv6);
    auto connB = socket_wrapper::UdpDatagram("::1", 8001, socket_wrapper::IPv6);
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    connA.write(sent_packet, "::1", 8001);
    std::string sender_ip;
    auto received_packet = connB.read(100, &sender_ip);
    ASSERT_EQ(sender_ip, "::1");
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

#ifdef OPENSSL_FOUND

TEST(SSL, ReadInternet){
    using namespace socket_wrapper;
    auto s = ConditionalBufferedStream(
            BufferedStream(StreamFactory::CreateSecureTcpStreamToServer("2a00:1450:4001:829::2003", 443, IPv6), 1024));
    int on_newline_fd = s.createEventfdOnCondition(ConditionalBufferedStream::getDelimiterCondition('\n'));
    s.start();
    s.write("GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n");
    std::string response = s.readBlockingStr(on_newline_fd, 1000);
    std::cout << "Got response:" << response << "\n";
    ASSERT_FALSE(response.empty());
    std::cout << s.getStream().getCipherName() << std::endl;
    std::cout << s.getStream().getPublicKey() << std::endl;
}

TEST(SSL, Listener){/*
    using namespace socket_wrapper;
    Listener l = Listener("cert.pem","key.pem",4433,TEST_IP_VERSION);
    std::cout << "Waiting for new client" << std::endl;
    auto s = l.accept();
    auto condBuffered = ConditionalBufferedStream(BufferedStream(std::move(s), 1024));
    int nl = condBuffered.createEventfdOnCondition(ConditionalBufferedStream::getDelimiterCondition('\n'));
    condBuffered.start();
    std::string response = condBuffered.readBlockingStr(nl);
    std::cout << "Got response:" << response << "\n";*/
}

TEST(SSL, EncryptedConnect){
    using namespace socket_wrapper;
    Listener l = Listener("cert.pem","key.pem",4433,TEST_IP_VERSION);
    auto server = std::jthread([&](){
        try {
            std::cout << "Waiting for new client" << std::endl;
            auto c = ConditionalBufferedStream(BufferedStream(l.accept(), 1024));
            int nl = c.createEventfdOnCondition(ConditionalBufferedStream::getDelimiterCondition('\n'));
            c.start();
            auto s = c.readBlockingStr(nl, 2000);
            std::cout << "server Got response:" << s << "\n";
            c.write("ECHO:" + s);
        } catch (SocketException &e) {
            std::cerr << "Server error:" << e.what() << std::endl;
        }
    });
    try {
        std::cout << "Connecting to server..." << std::endl;
        auto s = ConditionalBufferedStream(
                BufferedStream(StreamFactory::CreateSecureTcpStreamToServer("127.0.0.1", 4433, IPv4), 1024));
        int on_newline_fd = s.createEventfdOnCondition(ConditionalBufferedStream::getDelimiterCondition('\n'));
        s.start();
        s.write("ABC\n");
        std:cout << "Waiting for response" << std::endl;
        std::string response = s.readBlockingStr(on_newline_fd, 1000);
        std::cout << "Got response:" << response << "\n";
        assert(response == "ECHO:ABC\n");
    }catch (SocketException &e){
        std::cerr << "Client error:" << e.what() << std::endl;
    }

    }
#endif