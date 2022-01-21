#pragma once

#include <string>
using std::string;

#include <map>
using std::map;

#include "Producer.h"
#include <cstdint>

namespace logport{

    class LogPort;

    class HttpProducer : public Producer {

        public:
            HttpProducer( const map<string,string>& settings, LogPort* logport, const string& undelivered_log, const string& targets_list, uint32_t batch_size = 1 );
            virtual ~HttpProducer() override;

            virtual void produce( const string& message ) override;
            virtual void openUndeliveredLog() override;  //must be called before the first message is produced
            virtual void poll( int timeout_ms = 0 ) override;

        protected:
            string targets_list;
            uint32_t batch_size = 1;

    };

}




