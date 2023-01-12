#ifndef RT_CONTROLLER_TEST_MAIN_H
#define RT_CONTROLLER_TEST_MAIN_H

#include <gtest/gtest.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <future>
#include <cstdio>
#include <sys/poll.h>
#include <arpa/inet.h>

#define ASSERT_POLL_TIMED_OUT(_fd)                                                              \
    {std::array<pollfd, 1> poll_fds = {{{.fd = _fd, .events = POLLIN, .revents = 0}}};          \
    ASSERT_EQ(::poll(poll_fds.data(), poll_fds.size(), 20),0);}

#define ASSERT_POLL_GOT_EVENT(_fd)                                                              \
    {uint64_t condition_response;                                                               \
    std::array<pollfd, 1> poll_fds = {{{.fd = _fd, .events = POLLIN, .revents = 0}}};           \
    const int poll_result = ::poll(poll_fds.data(), poll_fds.size(), 20);                       \
    if (poll_result == 0) {                                                                     \
     FAIL();                                                                                    \
    } else {                                                                                    \
    ::read(_fd, &condition_response, sizeof(condition_response));                 \
    ASSERT_GE(poll_result, 1);                                                                  }}
#define ASSERT_READ_EQ(stream, fd, expected) {auto current_buffer = stream->read(fd); \
    ASSERT_EQ(std::string(current_buffer.begin(), current_buffer.end()),expected);}
#define ASSERT_READ_BLOCKING_EQ(stream, fd, expected) {auto current_buffer = stream->readBlocking(fd); \
    ASSERT_EQ(std::string(current_buffer.begin(), current_buffer.end()),expected);}
#define ASSERT_READ_BLOCKING_STR_EQ(stream, fd, expected) {auto str = stream->readBlockingStr(fd); \
    ASSERT_EQ(str,expected);}
#define ASSERT_READ_NON_BLOCKING_EQ(stream, fd, expected) {auto current_buffer = stream->read(fd); \
    ASSERT_EQ(std::string(current_buffer.begin(), current_buffer.end()),expected);}

#endif //RT_CONTROLLER_TEST_MAIN_H
