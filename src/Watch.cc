#include "Watch.h"

#include "PreparedStatement.h"
#include "Database.h"
#include "Common.h"
#include "Observer.h"

#include "InotifyWatcher.h"
#include "KafkaProducer.h"


#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <errno.h>
#include <signal.h>


#include <iostream>
#include <iomanip>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>


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
        :id(0), file_offset(0), pid(-1), last_pid(-1)
    {   

    }

    Watch::Watch( const PreparedStatement& statement )
        :id(0), file_offset(0), pid(-1), last_pid(-1)
    {   

        this->id = statement.getInt64( 0 );
        this->watched_filepath = statement.getText( 1 );
        this->file_offset = statement.getInt64( 2 );
        this->brokers = statement.getText( 3 );
        this->topic = statement.getText( 4 );
        this->product_code = statement.getText( 5 );
        this->hostname = statement.getText( 6 );
        this->pid = statement.getInt32( 7 );

        this->undelivered_log_filepath = this->watched_filepath + "_undelivered";

    }

    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, const string& product_code, const string& hostname, int64_t file_offset, pid_t pid )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic), product_code(product_code), hostname(hostname), id(0), file_offset(file_offset), pid(pid), last_pid(-1)
    {

    }


    void Watch::bind( PreparedStatement& statement, bool skip_id ) const{

        int current_offset = 0;

        if( !skip_id ){
            statement.bindInt64( current_offset++, this->id );
        }
        
        statement.bindText( current_offset++, this->watched_filepath );
        statement.bindInt64( current_offset++, this->file_offset );
        statement.bindText( current_offset++, this->brokers );
        statement.bindText( current_offset++, this->topic );
        statement.bindText( current_offset++, this->product_code );
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





    pid_t Watch::start( Observer& observer ){

        pid_t pid = fork();

        if( pid == 0 ){

            //child

            observer.addLogEntry( "Starting watch: " + this->watched_filepath );

            int exit_code = 0;

            {
                Database db;

                KafkaProducer kafka_producer( this->brokers, this->topic, this->undelivered_log_filepath );  

                InotifyWatcher watcher( db, kafka_producer, *this, observer );  //expects undelivered log to exist
                inotify_watcher_ptr = &watcher;

                //register signal handler
                signal( SIGINT, signal_handler_stop );
                signal( SIGTERM, signal_handler_stop );

                try{
                     watcher.startWatching(); //main loop; blocks
                     observer.addLogEntry( "logport: watcher.watch completed: id(" + logport::to_string<int64_t>(this->id) + ") " + this->watched_filepath );
                     exit_code = 0;
                }catch( std::exception &e ){
                     observer.addLogEntry( "logport: watcher.watch exception: " + string(e.what()) );
                     exit_code = 1;
                }
            }
            
            //exit must be called after the kafka_producer destructs (and not before)
            exit(exit_code);


        }else if( pid == -1 ){

            //error

            if( errno ){

                observer.addLogEntry( "logport: Failed to fork: errno: " + logport::to_string<int>(errno) );

            }else{

                observer.addLogEntry( "logport: Failed to fork." );

            }
            this->pid = -1;


        }else{

            //parent

            //returns the pid of the child
            observer.addLogEntry( "logport: Started watch (PID: " + logport::to_string<pid_t>(pid) + ")" );

            this->pid = pid;

            Database db;
            this->savePid(db);

            return pid;

        }

        return -1;

    }




    void Watch::stop( Observer& observer ){

        int status;

        //observer.addLogEntry( "logport: watcher.watch completed: id(" + logport::to_string<int64_t>(this->id) + ") " + this->watched_filepath );


        //terminate gracefully first, then forcefully if graceful shutdown lasts longer than 20s
        if( kill(this->pid, SIGINT) == -1 ){
            observer.addLogEntry( "logport: failed to kill watch with SIGINT." );
            return;
        }
        
        observer.addLogEntry( "logport: attempting graceful watch shutdown..." );

        //sleep(20);
        sleep(7);
        
        //this waitpid will clear the /proc/PID filesystem record
        pid_t child_pid2 = waitpid(-1, &status, WUNTRACED | WNOHANG );
        observer.addLogEntry( "logport: child_pid2: " + logport::to_string<pid_t>(child_pid2) );
        //sleep(2);
        sleep(1);

        //watch does not respond to SIGINT in certain conditions
                            
            //check to see if this->pid is still running
            int getpgid_result = getpgid(this->pid);
            observer.addLogEntry( "logport: getpgid_result: " + logport::to_string<int>(getpgid_result) );
            bool watch_still_running = true;
            if( getpgid_result == -1 ){
                watch_still_running = false;
            }


            if( watch_still_running ){

                //verify the process name before killing SIGKILL
                const string verified_process_name = proc_status_get_name( this->pid );
                observer.addLogEntry( "logport: verified process name: " + verified_process_name );

                if( verified_process_name == "logport" ){
                    observer.addLogEntry( "logport: watch PID " + logport::to_string<pid_t>(this->pid) + " required a forceful exit." );
                    if( kill(this->pid, SIGKILL) == -1 ){
                        observer.addLogEntry( "logport: failed to kill watch " + logport::to_string<pid_t>(this->pid) + " with SIGKILL." );
                    }
                }else{
                    observer.addLogEntry( "logport: same PID found, but different program name." );
                }

            }else{

                observer.addLogEntry( "logport: watch PID " + logport::to_string<pid_t>(this->pid) + " has exited gracefully." );

            }

       

    }

}
