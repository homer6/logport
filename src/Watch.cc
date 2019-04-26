#include "Watch.h"
#include "PreparedStatement.h"


#include "InotifyWatcher.h"
#include "KafkaProducer.h"


static logport::InotifyWatcher* inotify_watcher_ptr;

static void signal_handler_stop( int /*sig*/ ){
    inotify_watcher_ptr->run = false;
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
        this->pid = statement.getInt32( 5 );
    }

    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, int64_t file_offset, pid_t pid )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic), id(0), file_offset(file_offset), pid(pid), last_pid(-1)
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
        statement.bindInt32( current_offset++, this->pid );

    }



    void Watch::savePid( Database& db ){

        PreparedStatement statement( db, "UPDATE watches SET pid = ? WHERE id = ? ;" );

        statement.bindInt32( 0, this->pid );
        statement.bindInt64( 0, this->id );

        statement.step();
        statement.reset();
        statement.clearBindings();

    }



    void Watch::saveOffset( Database& db ){

        PreparedStatement statement( db, "UPDATE watches SET file_offset = ? WHERE id = ? ;" );

        statement.bindInt64( 0, this->file_offset );
        statement.bindInt64( 0, this->id );

        statement.step();
        statement.reset();
        statement.clearBindings();

    }





    pid_t Watch::start( std::ofstream& log_file ){

        pid_t pid = fork();

        if( pid == 0 ){

            //child

            Database db;

            KafkaProducer kafka_producer( this->brokers, this->topic, this->undelivered_log_filepath );  

            InotifyWatcher watcher( db, this->watched_filepath, this->undelivered_log_filepath, kafka_producer );  //expects undelivered log to exist
            inotify_watcher_ptr = &watcher;

            //register signal handler
            signal( SIGINT, signal_handler_stop );

            watcher.watch( *this ); //main loop; blocks


        }else if( pid == -1 ){

            //error

            if( errno ){
                log_file << "logport: Failed to fork: " << strerror(errno) << endl;
            }else{
                log_file << "logport: Failed to fork."<< endl;
            }
            this->pid = -1;


        }else{

            //parent

            //returns the pid of the child
            log_file << "logport: Started watch (PID: " << pid << ")" << endl;

            this->pid = pid;

            Database db;
            this->savePid(db);

            return pid;

        }

        return -1;

    }




    void Watch::stop( std::ofstream& log_file ){

        int status;

        //terminate gracefully first, then forcefully if graceful shutdown lasts longer than 20s
        if( kill(this->pid, SIGINT) == -1 ){
            log_file << "logport: failed to kill watch with SIGINT." << endl;
            return;
        }
        log_file << "logport: attempting graceful watch shutdown..." << endl;
        sleep(20);
        
        //this waitpid will clear the /proc/PID filesystem record
        pid_t child_pid2 = waitpid(-1, &status, WUNTRACED | WNOHANG );
        log_file << "logport: child_pid2: " << child_pid2 << endl;
        sleep(2);

        //watch does not respond to SIGINT in certain conditions
                            
            //check to see if this->pid is still running
            int getpgid_result = getpgid(this->pid);
            log_file << "logport: getpgid_result: " << getpgid_result << endl;
            bool watch_still_running = true;
            if( getpgid_result == -1 ){
                watch_still_running = false;
            }


            if( watch_still_running ){

                //verify the process name before killing SIGKILL
                const string verified_process_name = proc_status_get_name( this->pid );
                log_file << "logport: verified process name: " << verified_process_name << endl;

                if( verified_process_name == "logport" ){
                    log_file << "logport: watch PID " << this->pid << " has required a forceful exit." << endl;
                    if( kill(this->pid, SIGKILL) == -1 ){
                        log_file << "logport: failed to kill watch " << this->pid << " with SIGKILL." << endl;
                    }
                }else{
                    log_file << "logport: same PID found, but different program name." << endl;
                }

            }else{

                log_file << "logport: watch PID " << this->pid << " has exited gracefully." << endl;

            }



    }

}
