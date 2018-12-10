#include "LogPort.h"
//#include <cppkafka/producer.h>


LogPort::LogPort( const string &kafka_connection_string, const vector<string> &watched_files)
        :kafka_connection_string(kafka_connection_string), watched_files(watched_files)
{

}



void LogPort::startWatching(){

    //connect to kafka

/*
    // Create the config
    cppkafka::Configuration config = {
            { "metadata.broker.list", this->kafka_connection_string }
    };

    // Create the producer
    cppkafka::Producer producer(config);

    for( string line; std::getline(std::cin, line); ){
        cout << line << endl;
        //producer.produce( MessageBuilder(topic_name).partition(0).payload(line) );
    }
*/


    //listen to files

    //setup signal handlers?

    //loop?

}
