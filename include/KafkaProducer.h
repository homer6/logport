#pragma once

#include <string>
using std::string;

#include <map>
using std::map;

#include <librdkafka/rdkafka.h>
#include "Producer.h"

namespace logport{

    class LogPort;


    /*
        Produces messages to kafka.
        You can only safely have one KafkaProducer instantiated (per process) due to the static variables.
    */
    class KafkaProducer : public Producer{

        public:
            KafkaProducer( const map<string,string>& settings, LogPort* logport, const string& undelivered_log, const string& brokers_list, const string& topic );
            virtual ~KafkaProducer() override;

            virtual void produce( const string& message ) override;
            virtual void openUndeliveredLog() override;  //must be called before the first message is produced
            virtual void poll( int timeout_ms = 0 ) override;

        protected:
            string brokers_list;
            string topic;

            rd_kafka_t *rk;             /* Producer instance handle */
            rd_kafka_topic_t *rkt;      /* Topic object */
            
            char errstr[512];           /* librdkafka API error reporting buffer */

    };

}


