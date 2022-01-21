#include "Producer.h"
#include "LogPort.h"

#include <unistd.h>

namespace logport{


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
