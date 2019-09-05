#include "InotifyWatcher.h"

#include <stdexcept>

#include <sys/inotify.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>

#include <algorithm>
#include <iostream>
#include <iterator>

#include "LevelTriggeredEpollWatcher.h"

#include "Common.h"
#include "Observer.h"
#include "LogPort.h"

#include "Database.h"
#include "Watch.h"

#include <iostream>
#include <iomanip>
using std::cout;
using std::cerr;
using std::endl;


namespace logport{


    // Display information from inotify_event structure
    // see: http://man7.org/tlpi/code/online/book/inotify/demo_inotify.c.html
    static void displayInotifyEvent( struct inotify_event *i ){

        printf("    wd =%2d; ", i->wd);
        if (i->cookie > 0)
            printf("cookie =%4d; ", i->cookie);

        printf("mask = ");
        if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
        if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
        if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
        if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
        if (i->mask & IN_CREATE)        printf("IN_CREATE ");
        if (i->mask & IN_DELETE)        printf("IN_DELETE ");
        if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
        if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
        if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
        if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
        if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
        if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
        if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
        if (i->mask & IN_OPEN)          printf("IN_OPEN ");
        if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
        if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
        printf("\n");

        if (i->len > 0)
            printf("        name = %s\n", i->name);

    }


    #define INOTIFY_EVENT_BUFFER_LENGTH (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

    #define LOG_READ_BUFFER_SIZE 64 * 1024


    InotifyWatcher::InotifyWatcher( Database& db, KafkaProducer &kafka_producer, Watch& watch, LogPort* logport )
        :db(db), run(true), watched_file(watch.watched_filepath), undelivered_log(watch.undelivered_log_filepath), kafka_producer(kafka_producer), watch(watch), logport(logport)
    {

        /* Create inotify instance; add watch descriptors */

        char error_string_buffer[1024];

        this->inotify_fd = inotify_init();   /* Create inotify instance */
        if( this->inotify_fd == -1 ){
            //fprintf(stderr, "%% Failed to create inotify instance: errno %d\n", errno );
            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to create inotify instance: errno " + string(error_string_buffer) );
        }


        if( (this->inotify_watch_descriptor = inotify_add_watch(inotify_fd, watched_file.c_str(), IN_ALL_EVENTS)) == -1 ){
            //fprintf(stderr, "%% Failed to add inotify watch descriptor: errno %d\n", errno );
            close( this->inotify_fd );

            sleep( 3 ); //pause until the file is created (to prevent busy waiting on logrotate)

            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to add inotify watch descriptor: errno " + string(error_string_buffer) + " for file: " + watched_file );
        }


    }


    InotifyWatcher::~InotifyWatcher(){

        close( this->inotify_fd );

    }



    void InotifyWatcher::startWatching(){

        char inotify_event_buffer[INOTIFY_EVENT_BUFFER_LENGTH] __attribute__ ((aligned(8)));
        ssize_t inotify_event_num_read;
        char *p;
        struct inotify_event *in_event;

        char error_string_buffer[1024];

        //sleep(2); //avoids intermittent race condition on rotate (before open())

        int watched_file_fd = open( this->watched_file.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOFOLLOW );
        if( watched_file_fd == -1 ){
            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to open log file: errno " + string(error_string_buffer) );
        }


        //set the offset to the last read position
            {

                int64_t current_file_size = get_file_size( this->watched_file );

                Observer observer;               
                observer.addLogEntry( "logport: starting to watch " + this->watched_file + " Filesize(" + logport::to_string<int64_t>(current_file_size) + ") SavedResumePoint(" +  logport::to_string<int64_t>(this->watch.file_offset) + ")" );

                if( this->watch.file_offset > current_file_size ){

                    observer.addLogEntry( "logport: file size is greater than watched offset for " + this->watched_file + ". Resetting to beginning of file." );

                    sleep(1);

                    //error is seeking, reset to 0
                    this->watch.file_offset = 0;
                    try{
                        Database db;
                        this->watch.saveOffset( db );
                    }catch( std::exception &e ){
                        observer.addLogEntry( "logport: failed to save offset for " + this->watched_file + " " + string(e.what()) );
                    }

                }else{

                    off64_t current_file_position = lseek64( watched_file_fd, this->watch.file_offset, SEEK_SET );

                    if( current_file_position == -1 ){

                        observer.addLogEntry( "logport: error seeking for " + this->watched_file + ". Resetting to beginning of file." );

                        sleep(1);

                        //error is seeking, reset to 0
                        this->watch.file_offset = 0;

                        try{
                            Database db;
                            this->watch.saveOffset( db );
                        }catch( std::exception &e ){
                            observer.addLogEntry( "logport: failed to save offset for " + this->watched_file + " " + string(e.what()) );
                        }
                        

                    }else{
                        
                        observer.addLogEntry( "logport: resuming seeking for " + this->watched_file + " at offset: " + logport::to_string<off64_t>(current_file_position) + ", filesize: " + logport::to_string<int64_t>(current_file_size) );

                    }

                }

            }





        char log_read_buffer[LOG_READ_BUFFER_SIZE];


        LevelTriggeredEpollWatcher epoll_watcher( this->inotify_fd );

        bool log_being_rotated = false;
        //bool log_being_modified = false;
        bool startup = true;
        bool try_read = false;
        bool replaying_undelivered_log = false;

        string previous_log_partial;





        //replay the undelivered log entries

            const string temp_log_file = undelivered_log + "_temp";

            //return 0 if file does not exist
            uint64_t undelivered_file_size = get_file_size( undelivered_log );

            if( undelivered_file_size > 0 ){
                //if undelivered_log file exists and is not empty

                if( file_exists(temp_log_file) ){
                    //temp log file exists; abort with error
                    throw std::runtime_error( "Error: temp undelivered_log exists. Aborting to prevent overwriting." );
                }

                //move the file (now that we know temp_log_file doesn't exist)
                int rename_result = rename( undelivered_log.c_str(), temp_log_file.c_str() );
                if( rename_result == -1 ){
                    snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
                    throw std::runtime_error( "Failed to rename undelivered_log: errno " + string(error_string_buffer) );
                }

                this->kafka_producer.openUndeliveredLog(); //must be called before first message; this is why we use a temp file above
                this->kafka_producer.produce( this->filterLogLine("starting up - replaying undelivered log") );

                //ingest the undelivered_log contents
                replaying_undelivered_log = true;

                this->undelivered_log_fd = open( temp_log_file.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOFOLLOW );
                if( this->undelivered_log_fd == -1 ){
                    snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
                    throw std::runtime_error( "Failed to open undelivered log temp file: errno " + string(error_string_buffer) );
                }

            }else{

                this->kafka_producer.openUndeliveredLog(); //must be called before first message; this is why we use a temp file above
                this->kafka_producer.produce( this->filterLogLine("starting up") );

            }


        // Listen for events.
        // If this->shutting_down == true (controlled by signal handler), this->run will be true
        while( this->run ){

            if( !startup && epoll_watcher.watch(1000) ){  //returns immediately if there are inotify events waiting; returns after 1000ms if no events;

                //if there are new events waiting on the inotify_fd, read the events
                    inotify_event_num_read = read( this->inotify_fd, inotify_event_buffer, INOTIFY_EVENT_BUFFER_LENGTH );
                    if( inotify_event_num_read == 0 ){
                        //fprintf( stderr, "%% read() from inotify fd returned 0!" );
                        throw std::runtime_error( "read() from inotify fd returned 0" );
                    } 

                    if( inotify_event_num_read == -1 ){
                        //fprintf( stderr, "%% read() errno %d", errno );
                        snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
                        throw std::runtime_error( "read() from inotify fd returned errno " + string(error_string_buffer) );
                    }

                    //printf("Read %ld bytes from inotify fd\n", (long) inotify_event_num_read);


                //process all of the inotify events

                    for( p = inotify_event_buffer; p < inotify_event_buffer + inotify_event_num_read; ){

                        in_event = (struct inotify_event *) p;

                        if( in_event->mask & IN_MOVE_SELF ){
                            //this is being logrotated
                            log_being_rotated = true;
                            try_read = true;
                        }

                        if( in_event->mask & IN_MODIFY || in_event->mask & IN_CLOSE_WRITE ){
                            //this is being modified
                            //log_being_modified = true;
                            try_read = true;
                        }

                        if( 0 ) displayInotifyEvent(in_event);

                        p += sizeof(struct inotify_event) + in_event->len;

                    }

                

            }else{

                //no events waiting on inotify_fd; timed out watching for 1000ms

            }


                

            if( startup || try_read || log_being_rotated ){

                //read some input from the log file

                    int current_fd;
                    if( replaying_undelivered_log ){
                        current_fd = this->undelivered_log_fd;
                    }else{
                        current_fd = watched_file_fd;
                    }


                    // read the next bytes

                    int bytes_read = read( current_fd, log_read_buffer, LOG_READ_BUFFER_SIZE );

                    if( bytes_read > 0 ){

                        string log_chunk( log_read_buffer, bytes_read );

                        //append previous, if applicable
                            if( previous_log_partial.size() ){
                                log_chunk = previous_log_partial + log_chunk;
                                previous_log_partial.clear();
                            }

                        //send multiple lines as multiple kafka messages (no need to batch because rdkafka batches internally)
                        //no partial line will ever be sent
                            string::iterator current_message_begin_it = log_chunk.begin();
                            string::iterator current_message_end_it = log_chunk.begin();

                            char current_char;

                            while( current_message_end_it != log_chunk.end() ){
                                                                    
                                current_char = *current_message_end_it; //safe to deref because "bytes_read > 0" above

                                if( current_char == '\n' ){

                                    string sent_message;
                                    sent_message.reserve( log_chunk.size() );

                                    //only copy if there's something to copy
                                    if( current_message_end_it != current_message_begin_it ){
                                        std::copy( current_message_begin_it, current_message_end_it, std::back_inserter(sent_message) );  //drops the new line    
                                    }

                                    

                                    if( sent_message.size() > 0 ){

                                        string filtered_log_line = this->filterLogLine( sent_message );

                                        //handle consecutive newline characters (by dropping them)
                                        this->kafka_producer.produce( filtered_log_line );
                                        
                                        //skips the new line
                                        current_message_end_it++;

                                        //re-sync the being and end iterators
                                        current_message_begin_it = current_message_end_it;

                                        /*
                                        std::cout << "log_chunk(" << log_chunk.size() << "): " << std::endl;
                                        std::cout << "unfiltered_message(" << sent_message.size() << "): " << sent_message << std::endl;
                                        std::cout << "filtered_sent_message(" << filtered_log_line.size() << "): " << filtered_log_line << std::endl;
                                        std::cout << "previous_log_partial(" << previous_log_partial.size() << "): " << previous_log_partial << std::endl;
                                        */
                                    }else{

                                        current_message_end_it++;
                                    
                                    }


                                }else{

                                    current_message_end_it++;

                                }

                            }

                            //if we're at the end and we have an incomplete message, copy remaining incomplete line
                            if( current_message_begin_it != current_message_end_it ){
                                previous_log_partial.reserve( log_chunk.size() );
                                std::copy( current_message_begin_it, log_chunk.end(), std::back_inserter(previous_log_partial) );
                            }



                    }else{

                        //no bytes read (EOF)

                        if( startup && !replaying_undelivered_log ){
                            //startup will continue until read = zero bytes
                            startup = false;
                        }

                        if( replaying_undelivered_log ){
                            //remove the temp file

                            int unlink_result = unlink( temp_log_file.c_str() );
                            if( unlink_result == -1 ){
                                snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
                                throw std::runtime_error( "Failed to remove temporary undelivered log. errno: " + string(error_string_buffer) );
                            }

                            replaying_undelivered_log = false;

                            //if there's any previous_log_partial left over, flush it before continuing
                            if( previous_log_partial.size() ){
                                string filtered_previous_log_partial = this->filterLogLine( previous_log_partial );
                                this->kafka_producer.produce( filtered_previous_log_partial );
                                this->kafka_producer.poll();
                                previous_log_partial.clear();
                            }

                            Observer observer;
                            observer.addLogEntry( "logport: Finished replaying undelivered log." );

                        }



                        if( log_being_rotated ){

                            this->run = false;  //to exit on logrotate (after all bytes are drained)
                            //ensure that logrotate has the `delaycompress` option so that trailing bytes are properly drained

                            //if there's any previous_log_partial left over, flush it before shutting down
                            if( previous_log_partial.size() ){
                                string filtered_previous_log_partial = this->filterLogLine( previous_log_partial );
                                this->kafka_producer.produce( filtered_previous_log_partial );
                                this->kafka_producer.poll();
                                previous_log_partial.clear();
                            }

                            //update and save the committed offset back to the first byte
                            this->watch.file_offset = 0;
                            try{
                                this->watch.saveOffset( this->db );
                            }catch( std::exception &e ){
                                Observer observer;
                                observer.addLogEntry( "logport: failed to save offset for " + this->watched_file + " " + string(e.what()) );
                            }

                            //sleep(5); //avoids intermittent race condition on rotate (before open())

                        }



                    }

                    //log_being_modified = false;
                    try_read = false;

            }



            /* A producer application should continually serve
             * the delivery report queue by calling rd_kafka_poll()
             * at frequent intervals.
             * Either put the poll call in your main loop, or in a
             * dedicated thread, or call it after every
             * rd_kafka_produce() call.
             * Just make sure that rd_kafka_poll() is still called
             * during periods where you are not producing any messages
             * to make sure previously produced messages have their
             * delivery report callback served (and any other callbacks
             * you register). */
            this->kafka_producer.poll();



            //save the unsent offset, if this is shutting down
            if( this->run == false ){                
                sleep(1);
                this->kafka_producer.poll();
                Database db;
                off64_t current_file_position = lseek64( watched_file_fd, 0, SEEK_CUR );
                this->watch.file_offset = current_file_position - previous_log_partial.size();

                Observer observer;
                try{
                    this->watch.saveOffset( db );
                    observer.addLogEntry( "logport: saved " + this->watch.watched_filepath + " offset on shutdown (" + logport::to_string<off64_t>(current_file_position) + ")" );
                }catch( std::exception &e ){
                    observer.addLogEntry( "logport: failed to save offset for " + this->watched_file + " " + string(e.what()) );
                }

            }


        } //end this->run



    }






    string InotifyWatcher::filterLogLine( const string& unfiltered_log_line ) const{

        return this->watch.filterLogLine( unfiltered_log_line );

    }



}
