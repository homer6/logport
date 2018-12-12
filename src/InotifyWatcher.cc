#include "InotifyWatcher.h"

#include <stdexcept>

#include <sys/inotify.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>


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

    //listen to events
    while( this->run ){

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

        /* Process all of the events in buffer returned by read() */

        for( p = inotify_event_buffer; p < inotify_event_buffer + inotify_event_num_read; ){
            in_event = (struct inotify_event *) p;
            displayInotifyEvent(in_event);
            p += sizeof(struct inotify_event) + in_event->len;
        }

        const string message = "hello";

        if( message.size() == 0 ){
            /* only serve delivery reports */
            this->kafka_producer.poll();
            continue;
        }

        continue;


        this->kafka_producer.produce( message );


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

    }



}
