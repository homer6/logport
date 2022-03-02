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

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include "Common.h"

#include <chrono>
#include <thread>


namespace logport{





    HttpProducer::HttpProducer( const map<string,string>& initial_settings, LogPort* logport, const string &undelivered_log, const string& targets_list )
        :Producer( ProducerType::HTTP, {}, logport, undelivered_log ), targets_list(targets_list), targets_url_list(targets_list)
    {

        map<string,string> http_settings;
        //when these settings are set with 'logport set <key> <value>', they'll have the "http.producer." prefix on them.
        //eg. "logport set http.producer.batch.num.messages 100"
        //    "logport settings"
        http_settings["message.timeout.ms"] = "5000";   //5 seconds; this must be shorter than the timeout for flush below (in the deconstructor) or messages will be lost and not recorded in the undelivered_log
        http_settings["batch.num.messages"] = "1000";
        http_settings["compress"] = "true";  //gzip request
        http_settings["format"] = "application/json";
        http_settings["metadata"] = "{}"; //json metadata that will be sent with each message (if keys set)


        //copy over the overridden logport http producer settings
        for( const auto& [key, value] : initial_settings ){
            //cout << "Initial: key(" << key << ") value(" << value << ")" << endl;
            const string key_prefix = key.substr(0,14);  //"http.producer."
            if( key_prefix == "http.producer." ){
                string http_setting_key = key.substr(14, string::npos);
                if( http_setting_key.size() > 0 ){
                    http_settings[ http_setting_key ] = value;
                }
            }
        }
        this->settings = http_settings;

        bool compress = true;
        if( this->settings.count("compress") ){
            const string compress_str = this->settings.at("compress");
            if( compress_str == "false" ){
                compress = false;
            }
        }

        //create the connections

        uint32_t batch_size = 1000;
        try{
            const string batch_size_str = this->settings["batch.num.messages"];
            unsigned long batch_size_ul = std::stoul(batch_size_str);
            batch_size = static_cast<uint32_t>( batch_size_ul );
            if( batch_size < 1 ) batch_size = 1;
            if( batch_size > 100000 ) batch_size = 100000;
        }catch( std::exception& ){

        }

        json metadata = json::object();
        try{
            const string metadata_str = this->settings["metadata"];
            json metadata_temp = json::parse( metadata_str );
            if( !metadata_temp.is_object() ){
                throw std::runtime_error("Metadata JSON expects a JSON object.");
            }
            metadata = metadata_temp;
        }catch( std::exception& e ){
            //failed to parse metadata
            throw e;
        }



        FormatType format = FormatType::JSON;
        string format_str = this->settings.at("format");
        if( this->settings.count("format") ){
            if( format_str == "application/json" ){
                format = FormatType::KAFKA_JSON_V2_JSON;
            }else if( format_str == "application/vnd.kafka.json.v2+json" ){
                //see https://vectorized.io/blog/pandaproxy/ or https://docs.confluent.io/5.5.2/kafka-rest/api.html
                format = FormatType::KAFKA_JSON_V2_JSON;
            }else{
                format_str = string("application/json");
            }
        }


        //pre-compute the values so they're not computed on each message
        for( const homer6::Url& url : this->targets_url_list.urls ){

            unsigned short port = url.getPort();
            const string scheme = url.getScheme();
            const string hostname = url.getHost();
            const bool secure = url.isSecure();
            const string path = url.getPath();
            const string full_path = url.getFullPath();  //path + query + fragment


            HttpConnection connection;

            connection.url = url;
            connection.format = format;
            connection.format_str = format_str;
            connection.secure = secure;
            connection.compress = compress;
            connection.request_headers_template = httplib::Headers{
                { "Host", hostname },
                { "User-Agent", "logport" }
            };

            const string username = connection.url.getUsername();
            const string password = connection.url.getPassword();
            if( username.size() ){
                const string basic_auth_credentials = logport::encodeBase64( username + ":" + password );
                connection.request_headers_template.insert( { "Authorization", "Basic " + basic_auth_credentials } );
            }

            connection.full_path_template = connection.url.getFullPath();
            connection.batch_size = batch_size;

            if( connection.secure ){
                connection.https_client = std::make_unique<httplib::SSLClient>( hostname, port );
                connection.https_client->set_keep_alive(true);
                connection.https_client->set_follow_location(true);
                connection.https_client->set_compress(connection.compress);
            }else{
                connection.client = std::make_unique<httplib::Client>( hostname, port );
                connection.client->set_keep_alive(true);
                connection.client->set_follow_location(true);
                connection.client->set_compress(connection.compress);
            }

            connection.metadata = metadata;

            this->connections.push_back( std::move(connection) );



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

                bool should_flush = false;

                {
                    //critical section on connection.messages
                    std::scoped_lock lock( this->model_mutex );
                    connection.messages.push_back( message );
                    if( connection.messages.size() == connection.batch_size ){
                        should_flush = true;
                    }
                }

                if( should_flush ){
                    //cout << "io flush" << endl;
                    this->flush( &connection );  //this will lock on the mutex too
                }

            }catch( const std::exception& e ){

                const string error_message = string("Logport: failed to send log lines to http target: ") + string(e.what());
                cerr << error_message << '\n';
                this->logport->getObserver().addLogEntry( error_message );

            }

        }


    }



    void HttpProducer::poll( int /*timeout_ms*/ ){

        //rd_kafka_poll( this->rk, timeout_ms );
        //cout << "poll" << endl;

        //std::this_thread::sleep_for( std::chrono::milliseconds(1000) );

        for( auto& connection : this->connections ){

            bool should_flush = false;

            {
                //critical section on connection.messages
                std::scoped_lock lock( this->model_mutex );
                if( connection.messages.size() > 0 ){
                    should_flush = true;
                }
            }

            if( should_flush ){
                //cout << "poll flush" << endl;
                this->flush( &connection );  //this will lock on the mutex too
            }

        }

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



    void HttpProducer::flush( HttpConnection* connection ){

        json batch_json = json::object();
        json messages_json = json::array();

        {
            //critical section on connection->messages
            std::scoped_lock lock( this->model_mutex );

            switch( connection->format ){

                case FormatType::KAFKA_JSON_V2_JSON:
                    for( const auto& message : connection->messages ){
                        json record = json::object();
                        json temp_object = json::parse(message);
                        if( connection->metadata.size() > 0 ){
                            for( const auto& [key, value] : connection->metadata.items() ){
                                temp_object[key] = value;
                            }
                        }
                        record["value"] = temp_object;
                        messages_json.push_back( std::move(record) );
                    }
                    batch_json["records"] = messages_json;

                    break;

                case FormatType::JSON:
                    for( const auto& message : connection->messages ){
                        messages_json.push_back( json::parse(message) );
                    }
                    batch_json["messages"] = messages_json;
                    batch_json["count"] = connection->messages.size();

                    break;

                default:
                    throw std::runtime_error("Unknown format.");

            };

            connection->messages.clear();

        }

        const string batch_str = batch_json.dump();

        this->pool.push_task([ connection, batch_str ]{
            if( connection->secure ){
                connection->https_client->Post( connection->full_path_template.c_str(), connection->request_headers_template, batch_str, connection->format_str.c_str() );
            }else{
                connection->client->Post( connection->full_path_template.c_str(), connection->request_headers_template, batch_str, connection->format_str.c_str() );
            }
        });

    }


}