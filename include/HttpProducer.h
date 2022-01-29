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

#include "httplib.hpp"

#include <chrono>
#include <thread>
#include <mutex>
#include "thread_pool.hpp"

namespace logport{

    class LogPort;




    class HttpProducer : public Producer {

        public:

            using http_client_ptr = std::unique_ptr<httplib::Client>;
            using https_client_ptr = std::unique_ptr<httplib::SSLClient>;
            using settings_map = std::map<string,string>;
            enum struct FormatType{
                JSON,
                KAFKA_JSON_V2_JSON
            };

            // this is used as kind of a pre-computed set so that these values don't need to be computed on each message
            struct HttpConnection{
                homer6::Url url;
                string full_path_template;
                uint32_t batch_size = 1;
                FormatType format = FormatType::JSON;
                string format_str;
                bool compress = true;
                bool secure = false;
                http_client_ptr client;
                https_client_ptr https_client;
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


            virtual void flush( HttpConnection* connection );

        protected:
            string targets_list;
            uint32_t batch_size = 1;

            homer6::UrlList targets_url_list;

            http_connection_list connections;

            thread_pool pool{20};
            std::mutex model_mutex;

    };

}




