#include <cstring>
#include <csignal>
#include <algorithm>
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/SocketException.h"

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
        } catch (SocketException ex) {
            if (ex.exception_type != SocketException::SOCKET_TERMINATION_REQUEST) {
                throw ex;
            }
        }
    }

    void ConditionalBufferedStream::processDataSendSignals(const std::vector<char> &data) {
        if(data.empty()){
            return;
        }
        std::lock_guard<std::recursive_mutex> lk(buffer_event_handlers_mtx);
        for (auto event_handler: buffer_event_handlers) {
            int bytes_read_by_condition = event_handler.condition(data);
            if (bytes_read_by_condition > 0) {
                {
                    std::lock_guard<std::recursive_mutex> lk(received_data_per_condition_mtx);
                    received_data_per_condition[event_handler.fd].push_back(
                            stream.PopFromBuffer(bytes_read_by_condition));
                }
                // trigger the event, by writing the number of bytes in the buffer are returned
                uint64_t semaphore_post = 1;
                if (write(event_handler.fd, &semaphore_post, sizeof(semaphore_post)) < 0) {
                    throw SocketException(SocketException::SOCKET_WRITE, errno);
                }
                // ensure that signals can be triggered multiple times per ::read
                processDataSendSignals(std::vector<char>(begin(data) + bytes_read_by_condition, end(data)));
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

}