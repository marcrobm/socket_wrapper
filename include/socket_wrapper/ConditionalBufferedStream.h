#include "socket_wrapper/BufferedStream.h"
#include <functional>

namespace socket_wrapper {
    /**
     * a buffer event condition, should return 0 in case of no action
     */
    using buffer_event_condition = std::function<int(const std::vector<char> &)>;

    /**
     * This Stream splits incoming data into multiple fragments depending on a condition,
     * use like shown below
     * auto s = ConditionalBufferedStream(stream);
     * s.createEventfdOnCondition(buffer_event_condition condition);
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
         * reads available data from buffer
         * @return data
         */
        std::vector<char> read(int condition_fd);
        static buffer_event_condition getDelimiterCondition(char c);
    private:
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