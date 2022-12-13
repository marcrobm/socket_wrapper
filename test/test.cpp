#include "test.h"
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/StreamFactory.h"

using namespace std;

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