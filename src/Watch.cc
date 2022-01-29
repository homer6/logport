#include "Watch.h"

#include "PreparedStatement.h"
#include "Database.h"
#include "Common.h"
#include "Observer.h"
#include "LogPort.h"

#include "InotifyWatcher.h"

#include "Producer.h"
#include "KafkaProducer.h"
#include "HttpProducer.h"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <errno.h>
#include <signal.h>

#include <memory>
using std::unique_ptr;


#include <iostream>
#include <iomanip>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>

#include "json.hpp"
using json = nlohmann::json;



static logport::InotifyWatcher* inotify_watcher_ptr;

static void signal_handler_stop( int sig ){
    
    inotify_watcher_ptr->run = false;

    logport::Observer observer;

    switch( sig ){
        case SIGINT: observer.addLogEntry( "logport: Watch received SIGINT. Shutting down." ); break;
        case SIGTERM: observer.addLogEntry( "logport: Watch received SIGTERM. Shutting down." ); break;
        default: observer.addLogEntry( "logport: Watch received unknown. Shutting down." );
    };
    
}





namespace logport{


    Watch::Watch()
        :id(0), file_offset(0), last_undelivered_size(0), pid(-1), last_pid(-1)
    {   

    }


    Watch::Watch( const string& brokers )
        :id(0), file_offset(0), last_undelivered_size(0), pid(-1), last_pid(-1)
    {

        this->setBrokers( brokers );

    }


    Watch::Watch( const PreparedStatement& statement )
        :id(0), file_offset(0), last_undelivered_size(0), pid(-1), last_pid(-1)
    {   

        this->id = statement.getInt64( 0 );
        this->watched_filepath = statement.getText( 1 );
        this->file_offset = statement.getInt64( 2 );
        this->producer_type_description = statement.getText( 3 );

        this->brokers = statement.getText( 4 );
        this->setBrokers( this->brokers );

        this->topic = statement.getText( 5 );

        try{
            this->product_code = statement.getText( 6 );
        }catch( std::runtime_error& e ){

        }
        try{
            this->log_type = statement.getText( 7 );
        }catch( std::runtime_error& e ){

        }

        this->hostname = statement.getText( 8 );
        this->pid = statement.getInt32( 9 );

        this->undelivered_log_filepath = this->watched_filepath + "_undelivered";

        //this is set by the "setBrokers" call above
        //this->setProducerType( from_producer_type_description(this->producer_type_description) );

    }


    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, const string& product_code, const string& log_type, const string& hostname, int64_t file_offset, pid_t pid )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic), product_code(product_code), log_type(log_type), hostname(hostname), id(0), file_offset(file_offset), pid(pid), last_pid(-1)
    {
        this->setBrokers( brokers );
    }


    void Watch::setBrokers( const string& brokers ){

        this->brokers = brokers;
        this->brokers_url_list = homer6::UrlList{ this->brokers };

        string scheme = this->brokers_url_list.getScheme(); //throws on mismatch; always lowercase
        if( scheme == "http" || scheme == "https" ){
            this->setProducerType( ProducerType::HTTP );
        }else{
            this->setProducerType( ProducerType::KAFKA );
        }

    }


    void Watch::setProducerType( ProducerType producer_type ){
        this->producer_type = producer_type;
        this->producer_type_description = from_producer_type(producer_type);
    }


    void Watch::bind( PreparedStatement& statement, bool skip_id ) const{

        int current_offset = 0;

        if( !skip_id ){
            statement.bindInt64( current_offset++, this->id );
        }
        
        statement.bindText( current_offset++, this->watched_filepath );
        statement.bindInt64( current_offset++, this->file_offset );
        statement.bindText( current_offset++, this->producer_type_description );
        statement.bindText( current_offset++, this->brokers );
        statement.bindText( current_offset++, this->topic );
        statement.bindText( current_offset++, this->product_code );
        statement.bindText( current_offset++, this->log_type );
        statement.bindText( current_offset++, this->hostname );
        statement.bindInt32( current_offset++, this->pid );

    }



    void Watch::savePid( Database& db ){

        PreparedStatement statement( db, "UPDATE watches SET pid = ? WHERE id = ? ;" );

        statement.bindInt32( 0, this->pid );
        statement.bindInt64( 1, this->id );

        statement.step();
        statement.reset();
        statement.clearBindings();

    }



    void Watch::saveOffset( Database& db ){

        PreparedStatement statement( db, "UPDATE watches SET file_offset = ? WHERE id = ? ;" );

        statement.bindInt64( 0, this->file_offset );
        statement.bindInt64( 1, this->id );

        statement.step();
        statement.reset();
        statement.clearBindings();

    }


    void Watch::loadOffset( Database& db ){

        Watch reference_watch = db.getWatchById( this->id );

        this->file_offset = reference_watch.file_offset;

    }





    pid_t Watch::start( LogPort* logport ){

        pid_t pid = fork();

        if( pid == 0 ){

            //child

            logport->getObserver().addLogEntry( "logport: Starting watch: " + this->watched_filepath );
            this->runNow( logport );


        }else if( pid == -1 ){

            //error

            if( errno ){

                logport->getObserver().addLogEntry( "logport: Failed to fork: errno: " + logport::to_string<int>(errno) );

            }else{

                logport->getObserver().addLogEntry( "logport: Failed to fork." );

            }
            this->pid = -1;


        }else{

            //parent

            //returns the pid of the child
            logport->getObserver().addLogEntry( "logport: Started watch (PID: " + logport::to_string<pid_t>(pid) + ")" );

            this->pid = pid;

            Database db;
            this->savePid(db);

            return pid;

        }

        return -1;

    }



    void Watch::runNow( LogPort* logport ){

        int exit_code = 0;

        try{

            sleep(2);

            Database db;
            map<string,string> settings = db.getSettings();
            unique_ptr<Producer> producer;

            switch( this->producer_type ){

                case ProducerType::KAFKA:
                    producer = std::make_unique<KafkaProducer>( settings, logport, this->undelivered_log_filepath, this->brokers, this->topic );
                    break;

                case ProducerType::HTTP:
                    producer = std::make_unique<HttpProducer>( settings, logport, this->undelivered_log_filepath, this->brokers );
                    break;

                default:
                    throw std::runtime_error( "Unknown producer type." );

            };

            sleep(1);

            InotifyWatcher watcher( db, *producer, *this, logport );  //expects undelivered log to exist
            inotify_watcher_ptr = &watcher;

            //register signal handler
            signal( SIGINT, signal_handler_stop );
            signal( SIGTERM, signal_handler_stop );

            try{
                watcher.startWatching(); //main loop; blocks
                logport->getObserver().addLogEntry( "logport: watcher.watch completed: id(" + logport::to_string<int64_t>(this->id) + ") " + this->watched_filepath );
                exit_code = 0;
            }catch( std::exception &e ){
                logport->getObserver().addLogEntry( "logport: watcher.watch exception: " + string(e.what()) );
                exit_code = 1;
            }

        }catch( std::exception &e ){

            const string error_message = string("logport: watcher.start general exception: ") + string(e.what());
            cerr << error_message << endl;
            logport->getObserver().addLogEntry( error_message );
            exit_code = 2;

        }

        //exit must be called after the kafka_producer destructs (and not before)
        exit(exit_code);

    }


    void Watch::stop( LogPort* logport ){

        int status;

        //observer.addLogEntry( "logport: watcher.watch completed: id(" + logport::to_string<int64_t>(this->id) + ") " + this->watched_filepath );


        //terminate gracefully first, then forcefully if graceful shutdown lasts longer than 20s
        if( kill(this->pid, SIGINT) == -1 ){
            logport->getObserver().addLogEntry( "logport: failed to kill watch with SIGINT." );
            return;
        }
        
        logport->getObserver().addLogEntry( "logport: attempting graceful watch shutdown..." );

        //sleep(20);
        sleep(7);
        
        //this waitpid will clear the /proc/PID filesystem record
        pid_t child_pid2 = waitpid(-1, &status, WUNTRACED | WNOHANG );
        logport->getObserver().addLogEntry( "logport: child_pid2: " + logport::to_string<pid_t>(child_pid2) );
        //sleep(2);
        sleep(1);

        //watch does not respond to SIGINT in certain conditions
                            
            //check to see if this->pid is still running
            int getpgid_result = getpgid(this->pid);
            logport->getObserver().addLogEntry( "logport: getpgid_result: " + logport::to_string<int>(getpgid_result) );
            bool watch_still_running = true;
            if( getpgid_result == -1 ){
                watch_still_running = false;
            }


            if( watch_still_running ){

                //verify the process name before killing SIGKILL
                const string verified_process_name = proc_status_get_name( this->pid );
                logport->getObserver().addLogEntry( "logport: verified process name: " + verified_process_name );

                if( verified_process_name == "logport" ){
                    logport->getObserver().addLogEntry( "logport: watch PID " + logport::to_string<pid_t>(this->pid) + " required a forceful exit." );
                    if( kill(this->pid, SIGKILL) == -1 ){
                        logport->getObserver().addLogEntry( "logport: failed to kill watch " + logport::to_string<pid_t>(this->pid) + " with SIGKILL." );
                    }
                }else{
                    logport->getObserver().addLogEntry( "logport: same PID found, but different program name." );
                }

            }else{

                logport->getObserver().addLogEntry( "logport: watch PID " + logport::to_string<pid_t>(this->pid) + " has exited gracefully." );

            }

    }




    string Watch::filterLogLine( const string& unfiltered_log_line ) const{

        string filtered_log_line = unfiltered_log_line;


        // add your pre-filtering code here
        size_t card_number_location = filtered_log_line.find( "\"card_number\":\"" );
        if( card_number_location != std::string::npos ){
            //card_number key found

            size_t redacted_location = filtered_log_line.find( "\"card_number\":\"XXX" );

            if( redacted_location == std::string::npos ){
                //if unredacted credit_card found
                filtered_log_line = "{\"@timestamp\":" + get_timestamp() + ",\"log\":\"tombstone\"}";
                return filtered_log_line;
            }

        }
        


        size_t log_length = filtered_log_line.size();

        if( log_length == 0 ){
            return filtered_log_line;
        }

        json log_entry = json::object();

        log_entry["@timestamp"] = get_timestamp();
        if( this->hostname.size() ) log_entry["host"] = this->hostname;
        if( this->watched_filepath.size() ) log_entry["source"] = this->watched_filepath;
        if( this->product_code.size() ) log_entry["prd"] = this->product_code;
        if( this->log_type.size() ) log_entry["log_type"] = this->log_type;


        if( filtered_log_line[0] != '{' && filtered_log_line[0] != '[' ){
            log_entry["log"] = filtered_log_line;
        }else{
            try{
                json payload = json::parse( filtered_log_line );
                log_entry["log_obj"] = payload;
            }catch( std::exception& e ){
                log_entry["log"] = filtered_log_line;
            }
        }



        /*
        string json_meta = "{\"@timestamp\":" + get_timestamp() + ",\"host\":\"" + this->hostname + "\",\"source\":\"" + this->watched_filepath + "\",\"prd\":\"" + this->product_code + "\"";


        //unstructured single-line log entry
            if( filtered_log_line[0] != '{' ){

                filtered_log_line = json_meta + ",\"log\":\"" + escape_to_json_string(filtered_log_line) + "\"}";
                return filtered_log_line;

            }

        //embedded single-line JSON
            if( filtered_log_line[0] == '{' ){

                //this embedded single-line JSON MUST begin and end with a brace
                filtered_log_line = json_meta + ",\"log_obj\":" + filtered_log_line + "}";
                return filtered_log_line;

            }
        */

        return log_entry.dump();

    }


}
