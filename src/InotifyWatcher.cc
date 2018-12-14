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



InotifyWatcher::InotifyWatcher( const string &watched_file, KafkaProducer &kafka_producer )
    :run(1), watched_file(watched_file), kafka_producer(kafka_producer)
{

    /* Create inotify instance; add watch descriptors */

    char error_string_buffer[128];

    this->inotify_fd = inotify_init();   /* Create inotify instance */
    if( this->inotify_fd == -1 ){
        //fprintf(stderr, "%% Failed to create inotify instance: errno %d\n", errno );
        snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
        throw std::runtime_error( "Failed to create inotify instance: errno " + string(error_string_buffer) );
    }
        

    if( (this->inotify_watch_descriptor = inotify_add_watch(inotify_fd, watched_file.c_str(), IN_ALL_EVENTS)) == -1 ){
        //fprintf(stderr, "%% Failed to add inotify watch descriptor: errno %d\n", errno );
        close( this->inotify_fd );

        snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
        throw std::runtime_error( "Failed to add inotify watch descriptor: errno " + string(error_string_buffer) );
    }


}


InotifyWatcher::~InotifyWatcher(){

    close( this->inotify_fd );

}



void InotifyWatcher::watch(){

    char inotify_event_buffer[INOTIFY_EVENT_BUFFER_LENGTH] __attribute__ ((aligned(8)));
    ssize_t inotify_event_num_read;
    char *p;
    struct inotify_event *in_event;

    char error_string_buffer[128];


    int watched_file_fd = open( watched_file.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOFOLLOW );
    if( watched_file_fd == -1 ){
        snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
        throw std::runtime_error( "Failed to open log file: errno " + string(error_string_buffer) );
    }

    char log_read_buffer[LOG_READ_BUFFER_SIZE];


    LevelTriggeredEpollWatcher epoll_watcher( this->inotify_fd );

    bool log_being_rotated = false;
    //bool log_being_modified = false;
    bool startup = true;
    bool try_read = false;

    string previous_log_partial;


    //listen to events
    while( this->run ){

        if( !startup && epoll_watcher.watch(1000) ){

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

                printf("Read %ld bytes from inotify fd\n", (long) inotify_event_num_read);


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

                    displayInotifyEvent(in_event);

                    p += sizeof(struct inotify_event) + in_event->len;

                }

            

        }else{

            //no events waiting on inotify_fd; timed out watching for 1000ms

            // only serve delivery reports
            this->kafka_producer.poll();

        }


            

        if( startup || try_read || log_being_rotated ){

            //read some input from the log file

                int bytes_read = read( watched_file_fd, log_read_buffer, LOG_READ_BUFFER_SIZE );

                if( bytes_read > 0 ){

                    string message( log_read_buffer, bytes_read );

                    //append previous, if applicable
                        if( previous_log_partial.size() ){
                            message = previous_log_partial + message;
                            previous_log_partial.clear();
                        }

                    //strip incomplete (from the last newline character), if applicable (placing the partial in `previous_log_partial`)
                    //this will send multiple lines in a single message (batching)
                    //no partial line will ever be sent
                        string::iterator previous_it, current_it = message.end();
                        char current_char;
                        int next_length = -1;

                        while( current_it != message.begin() ){

                            previous_it = current_it;
                            current_it--;

                            next_length++;
                            current_char = *current_it;

                            if( current_char == '\n' ){

                                string sent_message;
                                sent_message.reserve( message.size() );
                                std::copy( message.begin(), current_it, std::back_inserter(sent_message) ); //drops the new line

                                if( next_length > 0 ){
                                    previous_log_partial.reserve( message.size() );
                                    std::copy( previous_it, message.end(), std::back_inserter(previous_log_partial) );
                                }

                                if( sent_message.size() > 0 ){
                                    //handle consecutive newline characters (by dropping them)
                                    this->kafka_producer.produce( sent_message );
                                    this->kafka_producer.poll();
                                }

                                //std::cout << "message(" << message.size() << "): " << message << std::endl;
                                //std::cout << "sent_message(" << sent_message.size() << "): " << sent_message << std::endl;
                                //std::cout << "previous_log_partial(" << previous_log_partial.size() << "): " << previous_log_partial << std::endl;

                                break;

                            }

                        }

                        if( current_it == message.begin() ){
                            //no newline found
                            previous_log_partial += message;
                        }

                }else{

                    if( startup ){
                        //startup will continue until read = zero bytes
                        startup = false;
                    }

                    if( log_being_rotated ){
                        this->run = false;
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
        //this->kafka_producer.poll();

    }



}
