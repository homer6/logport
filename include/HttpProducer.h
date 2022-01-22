#pragma once

#include <string>
using std::string;

#include <map>
using std::map;

#include <memory>
#include <utility>

#include "UrlList.h"

#include "Producer.h"
#include <cstdint>

#include "httplib.h"

namespace logport{

    class LogPort;

    using http_client_ptr = std::unique_ptr<httplib::SSLClient>;
    using http_connection = std::pair<homer6::Url,http_client_ptr>;
    using http_connection_list = std::vector<http_connection>;

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

            homer6::UrlList targets_url_list;

            http_connection_list connections;

    };

}




