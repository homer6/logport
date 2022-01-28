#pragma once

#include <map>
using std::map;

#include <string>
using std::string;


namespace logport {

    class LogPort;

    enum struct ProducerType{
        KAFKA,
        HTTP
    };

    string from_producer_type( ProducerType producer_type );
    ProducerType from_producer_type_description( const string& producer_type_description );


    /**
     * Base class for all producers.
     */
    class Producer {

        public:
            Producer(
                ProducerType type,
                const map <string, string> &settings,
                LogPort *logport,
                const string &undelivered_log
            );

            virtual ~Producer();


            /**
             * Produce a raw message to this source.
             *
             * @throws on failure
             * @returns on success
             * @blocks on queue full (logport does not have buffers so as to provide tight coupling with inotify, which deterministically applies backpressure)
             *
             * @param message unescaped (raw) message
             */
            virtual void produce( const string &message ) = 0;


            //void produceBatch() rd_kafka_produce_batch  TODO:implement

            //TODO: implement rd_kafka_set_logger

            virtual void openUndeliveredLog() = 0;  //must be called before the first message is produced

            virtual void poll( int timeout_ms = 0 ) = 0;  //called intermittently on another thread

        protected:
            ProducerType type = ProducerType::KAFKA;
            map<string, string> settings;
            LogPort *logport;

            int undelivered_log_fd = -1;
            string undelivered_log;
            bool undelivered_log_open = false;

    };


}