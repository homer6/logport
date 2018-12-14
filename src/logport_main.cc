#include <signal.h>
#include <string.h>

#include <stdexcept>
#include <iostream>

using std::cout;
using std::endl;
using std::cerr;


#include "InotifyWatcher.h"
#include "KafkaProducer.h"



#include <unistd.h>

static InotifyWatcher* inotify_watcher_ptr;

/**
 * @brief Signal termination of program
 */
static void stop( int /*sig*/ ){
    inotify_watcher_ptr->run = 0;
    cout << "stopping" << endl;
}





int main (int argc, char **argv) {

        if( argc != 5 ){
            cerr << "Usage: " << argv[0] << " <bootstrap-brokers-list> <topic> <file-to-watch> <undelivered-log>" << endl;
            cerr << "See: https://github.com/homer6/logport" << endl;
            return 1;
        }


        try{

            string brokers_list( argv[1] );
            string topic( argv[2] );
            string file_to_watch( argv[3] );
            string undelivered_log( argv[4] );

            /* Signal handler for clean shutdown */
            signal(SIGINT, stop);


            KafkaProducer kafka_producer( brokers_list, topic, undelivered_log );  

            InotifyWatcher watcher( file_to_watch, undelivered_log, kafka_producer );  //expects undelivered log to exist
            inotify_watcher_ptr = &watcher;


            /*
            //open the log file
            int log_file_fd = open( file_to_watch.c_str() );

            //determine the filesize
            off64_t end_of_file_position = lseek64( log_file_fd, 0, SEEK_END );

            //read the last_confirmed_sent (the last offset confirmed as received from kafka)
            off64_t last_confirmed_position = 0;

            //reset the description offset to last_confirmed_position
            off64_t current_file_position = lseek64( log_file_fd, last_confirmed_position, SEEK_SET );

            //send difference from last_confirmed_position and current_file_position to kafka before starting to listen with inotify
            */

            watcher.watch(); //main loop; blocks


            //send all messages after a Logrotate event or file move or anything that results in a IN_IGNORED event (eg. FS unmounted)


            //set the last_confirmed_sent back to 0 on a IN_DELETE_SELF event


            //logport will exit after a logrotate event
            //tmlogger will then restart it


        }catch( std::exception& e ){

            cerr << "Exception caught: " << e.what() << endl;
            return 1;

        }



        return 0;
}


