#ifndef EZNETWORK_STREAM_H
#define EZNETWORK_STREAM_H

#include <cstddef>
#include <mutex>
#include "StreamFactory.h"
#include "Listener.h"
#include "BaseTypes.h"
#include <sys/socket.h>

namespace socket_wrapper {
/**
 * @brief A wrapper class for c streams, following RAII principles
 */
    class Stream {
        friend StreamFactory;
        friend ListenerBase;
    public:
        /**
         * a Stream should not be created without an underlying socket
         */
        Stream() = delete;

        /**
         * (copy constructor)
         * Copying Streams is not allowed, if you want to copy a Stream, pass it by reference, or std::move it
         */
        Stream(Stream const &) = delete;

        /**
         * (move constructor)
         * Moves one Stream into a different Stream object.
         * @param src the Stream assigned to this Stream
         */
        Stream(Stream &&src) noexcept;

        /**
         * closes managed socket
         */
        ~Stream() noexcept;

        /**
         * (move assignment)
         * Moves one stream into another stream
         * @param stream_to_assign the stream steam to move
         * @return
         */
        Stream &operator=(Stream &&stream_to_assign) noexcept;

        /**
         * Reads a maximum of max_bytes_to_read from the underlying socket
         * @param buffer the buffer to read the data into
         * @param max_bytes_to_read the maximum number of bytes to read (has to be <= buffer size)
         * @param min_bytes_to_read the minimum number of bytes to read, function returns if min_bytes_to_read bytes
         *                          have been read from the socket
         * @return the number of bytes read
         * @throws SocketException in case of read errors
         */
        size_t read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms = -1);


        /**
         * A function to write raw data to a stream
         * @param buffer a pointer to the buffer to write to the socket
         * @param size the length of the buffer to write
         * @param attempts the number of attempt to write the buffer to the Stream
         */
        void write(char const *buffer, size_t size, int attempts);


        /**
         * aborts all currently running reads on the Stream
         */
        void stopReads();
    private:
        /**
        * creates a Stream object managing the file descriptor, should only be called by StreamFactory
        * @param socket_fd
        */
        explicit Stream(int socket_fd);
        struct sockaddr destination_addr; // the sockaddr this Stream is connected to
        int stream_file_descriptor;

        std::mutex stream_file_descriptor_write_mtx;
        std::mutex stream_file_descriptor_read_mtx;
        static int const kInvalidSocketFdMarker = -1;
        static int64_t const kSocketRetryIntervallMs = 50;
        std::atomic<int> stop_all_operations_event_fd;
    };
}

#endif // EZNETWORK_STREAM_H