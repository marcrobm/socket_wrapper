#include "test.h"
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/StreamFactory.h"
#include "socket_wrapper/UdpDatagram.h"
#include "socket_wrapper/BaseTypes.h"

using namespace std;
#define TEST_IP_VERSION socket_wrapper::IPv4

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


TEST(Datagram, SendthenRead) {
    auto conn = socket_wrapper::UdpDatagram("127.0.0.0", 8001, TEST_IP_VERSION);
    std::string message = "SomeTestString";
    auto sent_packet = std::vector<char>(message.begin(), message.end());
    conn.write(sent_packet, "127.0.0.0", 8001);
    auto received_packet = conn.read(100);
    ASSERT_EQ(received_packet,sent_packet);
}
