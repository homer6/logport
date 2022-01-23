#pragma once

#include <string>
using std::string;

#include <map>
using std::map;

#include <vector>
using std::vector;

#include <memory>
//#include <utility>

#include "UrlList.h"

#include "Producer.h"
#include <cstdint>

#include "httplib.h"

namespace logport{

    class LogPort;




    class HttpProducer : public Producer {

        public:

            using http_client_ptr = std::unique_ptr<httplib::SSLClient>;
            using settings_map = std::map<string,string>;

            struct HttpConnection{
                homer6::Url url;
                string full_path_template;
                uint32_t batch_size = 1;
                http_client_ptr client;
                httplib::Headers request_headers_template;
                settings_map settings;
                vector<string> messages;
            };

            using http_connection_list = std::vector<HttpConnection>;





            HttpProducer( const map<string,string>& settings, LogPort* logport, const string& undelivered_log, const string& targets_list );
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




