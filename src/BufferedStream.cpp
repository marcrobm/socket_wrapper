#include <algorithm>
#include <cstring>
#include "socket_wrapper/BufferedStream.h"
#include "socket_wrapper/SocketException.h"
namespace socket_wrapper {

    BufferedStream::BufferedStream(Stream src_stream, size_t buffer_size) : stream(std::move(src_stream)),
                                                                            stream_buffer_size(buffer_size),
                                                                            buffer_write_offset(0) {
        stream_buffer = (char *) calloc(buffer_size, 1);
    }
    BufferedStream::BufferedStream(BufferedStream &&src) noexcept: stream(std::move(src.stream)),stream_buffer_size(src.stream_buffer_size),buffer_write_offset(src.buffer_write_offset),stream_buffer(src.stream_buffer) {
        src.stream_buffer = (char*)-1; // invalidate the pointer
    }

    std::vector<char> BufferedStream::readUntilDelimiter(char delimiter_sequence) {
        while (buffer_write_offset < stream_buffer_size) {
            buffer_write_offset += stream.read(stream_buffer + buffer_write_offset,
                                               stream_buffer_size - buffer_write_offset, 1);
            auto delimiter_pos = std::find(stream_buffer, stream_buffer + buffer_write_offset, delimiter_sequence);
            if (delimiter_pos != stream_buffer + buffer_write_offset) {
                // we found the delimiter
                auto result = std::vector<char>(stream_buffer, delimiter_pos);
                buffer_write_offset -= result.size()+1;// -1 to account for the delimiter
                memmove(stream_buffer, delimiter_pos + 1, buffer_write_offset);
                return result;
            }
        }
        throw std::logic_error("could not find delimiter, even though the buffer is full");
    }

    std::vector<char> BufferedStream::read(size_t bytes_to_read, int timeout_ms) {
        // first we have to ensure there is enough data in the buffer
        if( bytes_to_read > stream_buffer_size){
            throw std::logic_error("buffer is to small for the requested read");
        }
        while (buffer_write_offset < bytes_to_read) {
            readAvailableDataIntoBuffer(timeout_ms);
        }
        auto result = PopFromBuffer(bytes_to_read);
        return result;
    }

    std::vector<char> BufferedStream::PopFromBuffer(size_t bytes_to_read) {
        std::lock_guard<std::recursive_mutex> lk(buffer_lock);
        if(bytes_to_read>buffer_write_offset){
            throw std::out_of_range(std::string("the buffer does not contain enough data yet, should read ")+
            std::to_string(bytes_to_read) + " can read " + std::to_string(buffer_write_offset) );
        }
        auto result = std::vector<char>(stream_buffer, stream_buffer + bytes_to_read);
        buffer_write_offset -= result.size();
        memmove(stream_buffer, stream_buffer + bytes_to_read, buffer_write_offset);
        return result;
    }

    size_t BufferedStream::readAvailableDataIntoBuffer(int timeout_ms) {
        std::lock_guard<std::recursive_mutex> lk(buffer_lock);
        size_t read_bytes = stream.read(stream_buffer + buffer_write_offset,
                                       stream_buffer_size - buffer_write_offset, 1,timeout_ms);
        buffer_write_offset += read_bytes;
        return read_bytes;
    }

    BufferedStream::~BufferedStream() noexcept {
        if(stream_buffer!=(char*)-1){
            free(stream_buffer);
        }
    }

    void BufferedStream::write(char const *buffer, size_t buffer_length) {
        stream.write(buffer, buffer_length,2);
    }

    size_t BufferedStream::read(char *buffer, size_t max_bytes_to_read, size_t min_bytes_to_read, int timeout_ms) {
        std::lock_guard<std::recursive_mutex> lk(buffer_lock);
        do {
            readAvailableDataIntoBuffer(timeout_ms);
        } while (buffer_write_offset<min_bytes_to_read);
        size_t read_data_length = std::min(buffer_write_offset,max_bytes_to_read);
        auto data = PopFromBuffer(read_data_length);
        for(auto d:data){
            *(buffer++) = d;
        }
        return read_data_length;
    }

    void BufferedStream::stopReads() {
        stream.stopReads();
    }


}