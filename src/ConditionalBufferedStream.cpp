#include <cstring>
#include <csignal>
#include "socket_wrapper/ConditionalBufferedStream.h"
#include "socket_wrapper/SocketException.h"

namespace socket_wrapper {

    void ConditionalBufferedStream::start() {
        worker = std::thread([&]() {
            try {
                while (true) { // exited through exception
                    int bytes_read = 0;
                    {// here we give read() has the opportunity to read data
                        try {
                            std::lock_guard<std::recursive_mutex> lk(buffer_mtx);
                            stream.readAvailableDataIntoBuffer(2);
                            for (auto event: buffer_event_handlers) {
                                std::vector<char> data = std::vector<char>(stream.stream_buffer,
                                                                           stream.stream_buffer +
                                                                           stream.buffer_write_offset);
                                uint64_t response = event.first(data);
                                if (response > 0) {
                                    // the condition is true, the event should be triggered, the number of bytes in the buffer are returned
                                    if (::write(event.second, &response, sizeof(response)) < 0) {
                                        throw SocketException(SocketException::SOCKET_WRITE, errno);
                                    }
                                }
                            }
                        } catch (SocketException ex) {
                            if (ex.exception_type != SocketException::SOCKET_READ_TIMEOUT) {
                                throw ex;
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // to allow others to access the buffer
                    }
                }
            } catch (SocketException ex) {
                if (ex.exception_type != SocketException::SOCKET_TERMINATION_REQUEST) {
                    throw ex;
                }
            }
        });
    }


    int ConditionalBufferedStream::createEventfdOnCondition(buffer_event_condition condition) {
        int fd = eventfd(0, 0);
        if (fd == -1) {
            throw std::runtime_error(std::string("Failed to create eventfd errno=") + std::strerror(errno));
        }
        buffer_event_handlers.emplace_back(condition, fd);
        return fd;
    }

    ConditionalBufferedStream::ConditionalBufferedStream(BufferedStream stream) : stream{std::move(stream)} {

    }

    ConditionalBufferedStream::~ConditionalBufferedStream() {
        stream.stopReads(); // results worker thread stopping
        worker.join();
    }

    std::vector<char> ConditionalBufferedStream::read(int count) {
        std::lock_guard<std::recursive_mutex> lk(buffer_mtx);
        return stream.PopFromBuffer(count);
    }

}