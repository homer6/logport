#ifndef LOGPORT_KAFKA_PRODUCER_H
#define LOGPORT_KAFKA_PRODUCER_H

#include <string>
using std::string;

#include <librdkafka/rdkafka.h>


class KafkaProducer{

    public:
        KafkaProducer( const string &brokers_list, const string &topic, const string &undelivered_log );
        ~KafkaProducer();

        void produce( const string& message ); //throws on failure

        //void produceBatch() rd_kafka_produce_batch  TODO:implement

        //TODO: implement rd_kafka_set_logger

        void openUndeliveredLog();  //must be called 


        void poll( int timeout_ms = 0 );

    protected:
        string brokers_list;
        string topic;

        int undelivered_log_fd;
        string undelivered_log;
        bool undelivered_log_open;

        rd_kafka_t *rk;             /* Producer instance handle */
        rd_kafka_topic_t *rkt;      /* Topic object */
        
        char errstr[512];           /* librdkafka API error reporting buffer */

};


#endif //LOGPORT_KAFKA_PRODUCER_H
