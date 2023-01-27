#include <cstring>
#include <csignal>
#include <algorithm>
#include <sys/poll.h>
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/SocketException.h"
#include "unistd.h"

namespace socket_wrapper {

    void ConditionalBufferedStream::start() {
        worker = std::thread([&]() { ConditionalBufferWorker(); });
    }

    void ConditionalBufferedStream::ConditionalBufferWorker() {
        try {
            while (true) { // exited through exception
                stream.readAvailableDataIntoBuffer();
                const std::vector<char> data = std::vector<char>(stream.stream_buffer,
                                                                 stream.stream_buffer +
                                                                 stream.buffer_write_offset);
                processDataSendSignals(data);
            }
        } catch (SocketException& ex) {
            if (ex.exception_type != SocketException::SOCKET_TERMINATION_REQUEST) {
                last_ex = ex.exception_type;
                std::lock_guard<std::recursive_mutex> lk(buffer_event_handlers_mtx);
                // Notify event handlers that the socket is no longer good
                for(auto x : buffer_event_handlers){
                    uint64_t semaphore_post = 1;
                    if (::write(x.fd, &semaphore_post, sizeof(semaphore_post)) < 0) {
                        throw SocketException(SocketException::SOCKET_WRITE, errno);
                    }
                }
            }
        }
    }

    void ConditionalBufferedStream::processDataSendSignals(const std::vector<char> &data) {
        if (data.empty()) {
            return;
        }
        std::lock_guard<std::recursive_mutex> lk(buffer_event_handlers_mtx);
        for (auto event_handler: buffer_event_handlers) {
            int bytes_read_by_condition = event_handler.condition(data);
            if(abs(bytes_read_by_condition) > data.size()){
                throw std::logic_error("a condition on a ConditionalBufferedStream tried to read more data, than exists");
            }
            if (bytes_read_by_condition != 0) {
                {
                    std::lock_guard<std::recursive_mutex> lk(received_data_per_condition_mtx);
                    auto read_data = stream.PopFromBuffer(abs(bytes_read_by_condition));
                    if(bytes_read_by_condition > 0){
                        received_data_per_condition[event_handler.fd].push_back(read_data);
                    }
                }
                if(bytes_read_by_condition > 0) {
                    // trigger the event, by writing the number of bytes in the buffer are returned
                    uint64_t semaphore_post = 1;
                    if (::write(event_handler.fd, &semaphore_post, sizeof(semaphore_post)) < 0) {
                        throw SocketException(SocketException::SOCKET_WRITE, errno);
                    }
                }
                // ensure that signals can be triggered multiple times per ::read
                processDataSendSignals(std::vector<char>(begin(data) + abs(bytes_read_by_condition), end(data)));
            }
        }
    }


    int ConditionalBufferedStream::createEventfdOnCondition(buffer_event_condition condition) {
        int fd = eventfd(0, EFD_SEMAPHORE);
        if (fd == -1) {
            throw std::runtime_error(std::string("Failed to create eventfd errno=") + std::strerror(errno));
        }
        buffer_event_handlers.emplace_back(buffer_event_handler{.condition = condition, .fd = fd});
        received_data_per_condition[fd] = {};
        return fd;
    }

    ConditionalBufferedStream::ConditionalBufferedStream(BufferedStream stream) : stream{std::move(stream)} {

    }

    ConditionalBufferedStream::~ConditionalBufferedStream() {
        for(auto x : buffer_event_handlers){::close(x.fd);}
        stream.stopReads(); // results worker thread stopping
        worker.join();
    }

    std::vector<char> ConditionalBufferedStream::read(int condition_fd) {
        std::lock_guard<std::recursive_mutex> lk(received_data_per_condition_mtx);
        if (received_data_per_condition.find(condition_fd) == end(received_data_per_condition)) {
            throw std::out_of_range("The condition_fd does not exist: " + std::to_string(condition_fd));
        }
        if (!received_data_per_condition[condition_fd].empty()) {
            std::vector<char> data = received_data_per_condition[condition_fd].front();
            received_data_per_condition[condition_fd].pop_front();
            return data;
        }if(last_ex != SocketException::SOCKET_OK){
            throw SocketException(last_ex);
        } else {
            throw std::out_of_range("A read on condition " + std::to_string(condition_fd) +
                                    " was performed, but there is no data present yet");
        }
    }

    buffer_event_condition ConditionalBufferedStream::getDelimiterCondition(char delimiter) {
        auto newline_condition = [delimiter](const std::vector<char> &data) {
            auto pos = std::find_if(begin(data), end(data), [delimiter](char c) { return c == delimiter; });
            return pos != end(data) ? (std::distance(begin(data), pos) + 1) : 0;
        };
        return newline_condition;
    }
    std::string ConditionalBufferedStream::readBlockingStr(int condition_fd, int timeout_ms) {
        auto read = readBlocking(condition_fd, timeout_ms);
        return {begin(read), end(read)};
    }
    std::vector<char> ConditionalBufferedStream::readBlocking(int condition_fd, int timeout_ms) {
        uint64_t condition_response;
        // Here we poll for either the condition or termination
        std::array<pollfd, 1> poll_fds = {
                {{.fd = condition_fd, .events = POLLIN, .revents = 0}},
        };
        const int poll_result = ::poll(poll_fds.data(), poll_fds.size(), timeout_ms);
        if (poll_result == 0) {
            throw SocketException(SocketException::SOCKET_POLL,errno);
        }else if (poll_fds[0].revents & POLLIN) {
            ::read(condition_fd, &condition_response, sizeof(condition_response));
        }else{
            throw SocketException(SocketException::SOCKET_READ_TIMEOUT,errno);
        }
        return read(condition_fd);
    }

    void ConditionalBufferedStream::write(std::vector<char> data) {
        stream.write(data.data(),data.size());
    }
    void ConditionalBufferedStream::write(std::string data) {
        write(std::vector<char>(begin(data), end(data)));
    }

}