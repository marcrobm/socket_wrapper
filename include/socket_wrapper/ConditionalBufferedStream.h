#ifndef SOCKET_WRAPPER_CONDITIONAL_BUFFERED_STREAM_H
#define SOCKET_WRAPPER_CONDITIONAL_BUFFERED_STREAM_H
#include "socket_wrapper/BufferedStream.h"
#include "SocketException.h"
#include <functional>

namespace socket_wrapper {
    /**
     * a buffer event condition, should return 0 in case of no action, otherwise the number of bytes to later be read should be returned
     * if the number is negative, the condition is not triggered, but the bytes are still removed from the buffer and discarded
     */
    using buffer_event_condition = std::function<int(const std::vector<char> &)>;

    /**
     * This Stream splits incoming data into segments depending on a condition,
     * Example where a stream is split into newlines and every line is read individually.
     * Furthermore poll() can be used on the fd returned by createEventfdOnCondition();
     * auto s = ConditionalBufferedStream(stream);
     * auto newline_condition = ConditionalBufferedStream::getDelimiterCondition('\n');
     * auto read_line_fd = s.createEventfdOnCondition(newline_condition);
     * s.start();
     * string line = s.readBlocking(read_line_fd);
     */
    class ConditionalBufferedStream {
    public:
        explicit ConditionalBufferedStream(BufferedStream stream);

        ~ConditionalBufferedStream();

        /**
         * created a Buffered Stream which triggers events, when specific conditions are met
         * @param condition
         * @return
         */
        int createEventfdOnCondition(buffer_event_condition condition);

        /**
         * starts processing the buffer, and triggering events, createEventfdOnCondition should no longer be called
         */
        void start();

        /**
         * reads available data, (non-blocking)
         * @param condition_fd the condition providing the data segments
         * @return the data segment read
         */
        std::vector<char> read(int condition_fd);
        /**
         * writes the characters of a string to the stream
         * @param data
         */
        void write(std::string data);
        /**
         * writes data to the stream
         * @param data the data to write
         */
        void write(std::vector<char> data);

        /**
         * performs a blocking read
         * @param condition_fd
         * @return
         */
        std::vector<char> readBlocking(int condition_fd, int timeout_ms = -1);
        /**
         * performs a blocking read
         * @param condition_fd
         * @return the read string
         */
        std::string readBlockingStr(int condition_fd, int timeout_ms = -1);
        static buffer_event_condition getDelimiterCondition(char c);

    private:
        SocketException::Type last_ex = SocketException::SOCKET_TERMINATION_REQUEST;
        std::thread worker;
        BufferedStream stream;

        struct buffer_event_handler {
            buffer_event_condition condition;
            int fd;
        };
        /**
         * all conditions waited for
         */
        std::list<buffer_event_handler> buffer_event_handlers;
        std::recursive_mutex buffer_event_handlers_mtx;

        std::map<int, std::list<std::vector<char>>> received_data_per_condition;
        std::recursive_mutex received_data_per_condition_mtx;

        void ConditionalBufferWorker();

        void processDataSendSignals(const std::vector<char> &data);
    };
}
#endif