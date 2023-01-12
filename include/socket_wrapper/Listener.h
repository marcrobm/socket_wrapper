#ifndef SOCKET_WRAPPER_LISTENER_H
#define SOCKET_WRAPPER_LISTENER_H

#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>
#include <memory>

#include "Stream.h"
#include "BaseTypes.h"

namespace socket_wrapper {
/**
 * @brief A wrapper class for accepting incoming TCP connections, following RAII principles
 */
    class ListenerBase {
    protected:
        ListenerBase() = delete;
        /**
         * Creates a Listener on a given port, incoming Streams are passed to onIncomingStream
         * @param onIncomingStream a callback for receiving incoming Streams
         * @param port the port to start the listener on
         * @param version The version of the ip (v4,v6) to use
         * @throws SocketException on errors
         */
        explicit ListenerBase(int port = 23, IP_VERSION version = socket_wrapper::IPv4);
        /**
         * closes the underlying socket, and thread
         */
        ~ListenerBase() noexcept;
        /**
         * should only be called once after construction is completed
         */
        void startAccepting();
        /**
        * should only be called before destruction
         * after this call onIncomingStream will no longer be called
        */
        void stopAccepting();
    private:
        virtual void onIncomingStream(Stream stream) = 0;
        std::thread handle_incoming_streams_task;
        std::atomic<int> listener_socket_fd;
        std::atomic<int> listener_end_fd;
        std::atomic<bool>stopped_accepting;
        void handleIncomingStreams();
    };
}

#endif
