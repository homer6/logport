#include "Producer.h"
#include "LogPort.h"

#include <unistd.h>

namespace logport{

    string from_producer_type( ProducerType producer_type ){

        switch( producer_type ){
            case ProducerType::KAFKA:
                return string("KAFKA" );
            case ProducerType::HTTP:
                return string("HTTP" );
        };

        throw std::runtime_error( "from_producer_type unknown producer type." );

    }

    ProducerType from_producer_type_description( const string& producer_type_description ){

        if( producer_type_description == "KAFKA" ){
            return ProducerType::KAFKA;
        }else if( producer_type_description == "HTTP" ){
            return ProducerType::HTTP;
        }

        throw std::runtime_error( "from_producer_type_description unknown producer type description." );

    }



    Producer::Producer( ProducerType type, const map<string,string>& settings, LogPort* logport, const string &undelivered_log )
        :type(type), settings(settings), logport(logport), undelivered_log(undelivered_log)
    {


    }


    Producer::~Producer(){

        if( this->undelivered_log_open ){
            close( this->undelivered_log_fd );
            this->undelivered_log_open = false;
        }

    }


}
