#include "socket_wrapper/BufferedStream.h"
#include <functional>

namespace socket_wrapper {
    /**
     * a buffer event condition, should return 0 in case of no action
     */
    using buffer_event_condition = std::function<uint64_t (std::vector<char>&)>;
    using buffer_event_handler = std::pair<buffer_event_condition, int>;

    /**
     * a class providing a easy way to read fragments from a stream, in a buffered way
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
         * reads currently available data
         * @return data
         */
        std::vector<char> read(int count);
    private:
        std::thread worker;
        BufferedStream stream;
        // locked when reading stream, and accessing buffer_event_handlers
        std::recursive_mutex buffer_mtx;
        std::list<buffer_event_handler> buffer_event_handlers;
        void sendApplicableBufferEvents();
    };
}