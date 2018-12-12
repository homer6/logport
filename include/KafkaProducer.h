#ifndef LOGPORT_KAFKA_PRODUCER_H
#define LOGPORT_KAFKA_PRODUCER_H

#include <string>
using std::string;

#include <librdkafka/rdkafka.h>


class KafkaProducer{

    public:
        KafkaProducer( const string &brokers_list, const string &topic, int32_t partition = -1 );
        ~KafkaProducer();

        void produce( const string& message ); //throws on failure

        //void produceBatch() rd_kafka_produce_batch  TODO:implement

        //TODO: implement rd_kafka_set_logger


        void poll( int timeout_ms = 0 );

    protected:
        string brokers_list;
        string topic;
        int32_t partition;

		rd_kafka_t *rk;             /* Producer instance handle */
		rd_kafka_topic_t *rkt;      /* Topic object */
		
		char errstr[512];           /* librdkafka API error reporting buffer */		

};


#endif //LOGPORT_KAFKA_PRODUCER_H
