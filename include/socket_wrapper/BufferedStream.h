#ifndef EZNETWORK_STREAMANALYZER_H
#define EZNETWORK_STREAMANALYZER_H

#include <vector>
#include <sys/eventfd.h>
#include <list>
#include "BaseTypes.h"
#include "Stream.h"

namespace socket_wrapper {
    /**
     * a class providing a easy way to read fragments from a stream, in a buffered way
     */
    class BufferedStream {
        friend ConditionalBufferedStream;
    public :
        BufferedStream() = delete;
        /**
         * creates a Buffer on top of a given stream, the stream has to be moved
         * @param src_stream the stream to encapsulate
         * @param buffer_size the size of the internal buffer.
         */
        BufferedStream(Stream src_stream, size_t buffer_size);

        /**
         * frees all internal buffers
         */
        ~BufferedStream() noexcept;

        /**
         * (move constructor)
         * Moves one Stream into a different Stream object.
         * @param src the Stream assigned to this Stream
         */
        BufferedStream(BufferedStream &&src) noexcept;

        /**
         * blocks execution until the delimiter has been read
         * @param delimiter_sequence
         * @return the characters read including the delimiter
         * @throws SocketException in case of invalid reads
         */
        std::vector<char> readUntilDelimiter(char delimiter_sequence);

        /**
         * reads a given number of bytes from the underlying stream
         * @param bytes_to_read the number of bytes to read
         * @return
         */
        std::vector<char> read(size_t bytes_to_read, int timeout_ms = -1);

        size_t read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms = -1);
        /**
         * Writes a given buffer to a Buffered Stream
         * @param buffer the data to write to the stream
         * @param buffer_length the length of the data to write
         */
        void write(char const *buffer, size_t buffer_length);
        /**
         * stops current reads, should only be called before destruction
         */
        void stopReads();
    private:
        Stream stream;
        std::recursive_mutex buffer_lock;
        size_t stream_buffer_size;
        char *stream_buffer;
        size_t buffer_write_offset; // the number of bytes currently in the buffer
        size_t readAvailableDataIntoBuffer(int timeout_ms = -1);
        std::vector<char> PopFromBuffer(size_t bytes_to_read);

    };
}
#endif
