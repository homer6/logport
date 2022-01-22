#include "HttpProducer.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <algorithm>

#include <errno.h>

#include <stdexcept>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "LogPort.h"

#include <httplib.h>

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include "Common.h"

#include "json.hpp"
using json = nlohmann::json;


namespace logport{



    HttpProducer::HttpProducer( const map<string,string>& initial_settings, LogPort* logport, const string &undelivered_log, const string& targets_list, uint32_t batch_size )
        :Producer( ProducerType::HTTP, {}, logport, undelivered_log ), targets_list(targets_list), batch_size(batch_size), targets_url_list(targets_list)
    {


        map<string,string> http_settings;
        http_settings["message.timeout.ms"] = "5000";   //5 seconds; this must be shorter than the timeout for flush below (in the deconstructor) or messages will be lost and not recorded in the undelivered_log
        http_settings["batch.num.messages"] = "1";

        //copy over the overridden logport http producer settings
        for( const auto& [key, value] : initial_settings ){
            const string key_prefix = key.substr(0,14);  //"http.producer."
            if( key_prefix == "http.producer." ){
                string http_setting_key = key.substr(14, string::npos);
                if( http_setting_key.size() > 0 ){
                    http_settings[ http_setting_key ] = value;
                }
            }
        }
        this->settings = http_settings;


        /*
        for( map<string,string>::iterator it = http_settings.begin(); it != http_settings.end(); it++ ){

            const string setting_key = it->first;
            const string setting_value = it->second;
            if( rd_kafka_conf_set(conf, setting_key.c_str(), setting_value.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK ){
                rd_kafka_conf_destroy(conf);
                throw std::runtime_error( string("HttpProducer: Failed to set configuration for ") + setting_key + ": " + errstr );
            }

        }
        */

        //create the connections

        for( const homer6::Url& url : this->targets_url_list.urls ){

            unsigned short port = url.getPort();
            const string hostname = url.getHost();
            const bool secure = url.isSecure();
            const string path = url.getPath();
            const string full_path = url.getFullPath();  //path + query + fragment

            HttpConnection connection;

            connection.request_headers_template = httplib::Headers{
                { "Host", hostname },
                { "User-Agent", "logport" }
            };

            const string username = url.getUsername();
            const string password = url.getPassword();
            if( username.size() ){
                const string basic_auth_credentials = logport::encodeBase64( username + ":" + password );
                connection.request_headers_template.insert( { "Authorization", "Basic " + basic_auth_credentials } );
            }

            connection.full_path_template = url.getFullPath();
            connection.batch_size = batch_size;

            if( secure ){
                connection.client = std::make_unique<httplib::SSLClient>( hostname, port );
                this->connections.push_back( std::move(connection) );
            }else{
                const string error_message("HTTP connections are not supported.");
                cerr << error_message << endl;
                this->logport->getObserver().addLogEntry( error_message );
                throw std::runtime_error(error_message);
                //std::unique_ptr http_client = std::make_unique<httplib::Client>( hostname, port );
                //this->connections.push_back( { url, std::move(http_client) } );
            }

        }


    }



    HttpProducer::~HttpProducer(){

        this->logport->getObserver().addLogEntry( "Flushing final HTTP messages." );

        //rd_kafka_flush(this->rk, 6 * 1000 /* wait for max 6 seconds */);
        //this wait must be longer than the message.timeout.ms in the conf above or the messages will be lost and not stored in the undelivered_log

        if( this->undelivered_log_open ){
            close( this->undelivered_log_fd );
            this->undelivered_log_open = false;
        }

        this->logport->getObserver().addLogEntry( "HTTP producer shutdown complete: " + this->undelivered_log );

    }



    void HttpProducer::produce( const string& message ){

        if( message.size() == 0 ) return;

        for( auto& connection : this->connections ){

            try{

//                if( connection.batch_size == 1 ) {
//                    connection.client->Post( connection.full_path_template.c_str(), connection.request_headers_template, message, "application/json" );
//                }else {
                    connection.messages.push_back( message );
                    if( connection.messages.size() == connection.batch_size ){

                        json messages_json = json::array();
                        for( const auto& message : connection.messages ){
                            messages_json.push_back( json::parse(message) );
                        }
                        json batch_json = json::object();
                        batch_json["messages"] = messages_json;
                        const string batch_str = batch_json.dump();

                        connection.client->Post( connection.full_path_template.c_str(), connection.request_headers_template, batch_str, "application/json" );
                        connection.messages.clear();

                    }
//                }

            }catch( const std::exception& e ){

                const string error_message = string("Logport: failed to send log lines to http target: ") + string(e.what());
                cerr << error_message << '\n';
                this->logport->getObserver().addLogEntry( error_message );

            }

        }


    }



    void HttpProducer::poll( int /*timeout_ms*/ ){

        //rd_kafka_poll( this->rk, timeout_ms );

    }



    void HttpProducer::openUndeliveredLog(){

        //O_APPEND is not used for undelivered_log because of NFS usage
        /*
        O_APPEND may lead to corrupted files on NFS filesystems if
        more than one process appends data to a file at once.  This is
        because NFS does not support appending to a file, so the
        client kernel has to simulate it, which can't be done without
        a race condition.
        */

        char error_string_buffer[1024];

        this->undelivered_log_fd = open( undelivered_log.c_str(), O_WRONLY | O_CREAT | O_LARGEFILE | O_NOFOLLOW, S_IRUSR | S_IWUSR ); //mode 0400
        if( this->undelivered_log_fd == -1 ){

            //rd_kafka_topic_destroy(this->rkt);
            //rd_kafka_destroy(this->rk);

            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to open undelivered log file for writing: errno " + string(error_string_buffer) );
        }

        this->undelivered_log_open = true;

    }



}