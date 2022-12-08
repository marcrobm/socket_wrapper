#include <sys/poll.h>
#include "test.h"
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/StreamFactory.h"
using namespace std;

TEST(ConditionalBufferedStream, ReadUntilDelimiter) {
    using namespace socket_wrapper;
    auto streams = StreamFactory::CreatePipe();
    auto delimiter_fd_stream = std::make_shared<ConditionalBufferedStream>(BufferedStream(std::move(streams[1]), 512));
    auto detected_abc = [](std::vector<char> &data) {
        auto pos = std::find_if(begin(data), end(data), [](char c) { return c == 'b'; });
        return pos != end(data) ? std::distance(begin(data),pos) : 0;
    };
    int fd_on_a_in_buffer = delimiter_fd_stream->createEventfdOnCondition(detected_abc);
    delimiter_fd_stream->start();
    std::array<pollfd, 1> poll_fds = {{{.fd = fd_on_a_in_buffer, .events = POLLIN, .revents = 0}}};
    uint64_t condition_response;
    auto AssertpollTimedOut = [&]() {
        const int poll_result = ::poll(poll_fds.data(), poll_fds.size(), 20);
        ASSERT_EQ(poll_result, 0); // timed out
    };
    auto AssertpollGotEvent = [&]() {
        const int poll_result = ::poll(poll_fds.data(), poll_fds.size(), 20);
        if(poll_result==0){
            ASSERT_NE(poll_result,0);
        }else{
            ::read(fd_on_a_in_buffer, &condition_response, sizeof(condition_response));
            ASSERT_GE(poll_result, 1); // got event
        }
    };
    streams[0].write("xyz", 3, 1);
    AssertpollTimedOut();
    streams[0].write("def", 3, 1);
    AssertpollTimedOut();
    streams[0].write("bab", 3, 1);
    AssertpollGotEvent();
    auto current_buffer = delimiter_fd_stream->read(condition_response); // this condition returns how many bytes should be read
    auto st = std::string(current_buffer.begin(), current_buffer.end());
    ASSERT_EQ(st, "xyzdef"); // excluding b excluded
    current_buffer = delimiter_fd_stream->read(3);
    st = std::string(current_buffer.begin(), current_buffer.end());
    ASSERT_EQ(st, "bab");

    // ::read(fd_on_a_in_buffer, &condition_response, sizeof(condition_response));
    AssertpollTimedOut();
    AssertpollTimedOut();
    streams[0].write("123", 3, 1);
    AssertpollTimedOut();
    streams[0].write("abc", 3, 1);
    AssertpollGotEvent();
    current_buffer = delimiter_fd_stream->read(6);
    st = std::string(current_buffer.begin(), current_buffer.end());
    ASSERT_EQ(st, "123abc");
}