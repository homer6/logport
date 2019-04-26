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

#include <sys/time.h>

#include "Common.h"

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


    InotifyWatcher::InotifyWatcher( Database& db, KafkaProducer &kafka_producer, Watch& watch, std::ofstream& log_file )
        :db(db), run(1), watched_file(watch.watched_filepath), undelivered_log(watch.undelivered_log_filepath), kafka_producer(kafka_producer), watch(watch), log_file(log_file)
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

            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to add inotify watch descriptor: errno " + string(error_string_buffer) );
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


        int watched_file_fd = open( this->watched_file.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOFOLLOW );
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
        bool replaying_undelivered_log = false;

        string previous_log_partial;





        //replay the undelivered log entries

            const string temp_log_file = undelivered_log + "_temp";

            if( access( undelivered_log.c_str(), F_OK ) != -1 ) {
                //if undelivered_log file exists

                if( access( temp_log_file.c_str(), F_OK ) != -1 ) {
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

                string filtered_log_line = this->filterLogLine("starting up - replaying undelivered log");
                this->kafka_producer.produce( filtered_log_line );

                //ingest the undelivered_log contents
                replaying_undelivered_log = true;

                this->undelivered_log_fd = open( temp_log_file.c_str(), O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOFOLLOW );
                if( this->undelivered_log_fd == -1 ){
                    snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
                    throw std::runtime_error( "Failed to open undelivered log temp file: errno " + string(error_string_buffer) );
                }

                this->kafka_producer.poll( 1000 );

            }else{

                this->kafka_producer.openUndeliveredLog(); //must be called before first message; this is why we use a temp file above
                string filtered_log_line = this->filterLogLine("starting up");
                this->kafka_producer.produce( filtered_log_line );

                this->kafka_producer.poll( 1000 );

            }




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

                    int current_fd;
                    if( replaying_undelivered_log ){
                        current_fd = this->undelivered_log_fd;
                    }else{
                        current_fd = watched_file_fd;
                    }


                    //seek to current offset
                        /*
                        if( !replaying_undelivered_log ){

                            //determine the filesize
                            //off64_t end_of_file_position = lseek64( current_fd, 0, SEEK_END );

                            //read the last_confirmed_sent (the last offset confirmed as received from kafka)
                            off64_t last_confirmed_position = watch.file_offset;

                            //reset the description offset to last_confirmed_position
                            if( startup ){
                                lseek64( current_fd, last_confirmed_position, SEEK_SET );
                            }else{
                                //offset cannot be beyond end of file
                                //watch.file_offset = 0;
                                //watch.saveOffset( this->db );
                            }
                            
                        }
                        */
                        


                    // read the next bytes

                    int bytes_read = read( current_fd, log_read_buffer, LOG_READ_BUFFER_SIZE );

                    if( bytes_read > 0 ){

                        string log_chunk( log_read_buffer, bytes_read );

                        //append previous, if applicable
                            if( previous_log_partial.size() ){
                                log_chunk = previous_log_partial + log_chunk;
                                previous_log_partial.clear();
                            }

                        //send multiple lines (newline character delimited), if applicable (placing the trailing partial in `previous_log_partial`)
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
                                        this->kafka_producer.poll();

                                        //update and save the committed offset
                                        //watch.file_offset += log_chunk.size();
                                        //watch.saveOffset( this->db );
                                        
                                        //skips the new line
                                        current_message_end_it++;

                                        //re-sync the being and end iterators
                                        current_message_begin_it = current_message_end_it;

                                        std::cout << "log_chunk(" << log_chunk.size() << "): " << std::endl;
                                        std::cout << "unfiltered_message(" << sent_message.size() << "): " << sent_message << std::endl;
                                        std::cout << "filtered_sent_message(" << filtered_log_line.size() << "): " << filtered_log_line << std::endl;
                                        std::cout << "previous_log_partial(" << previous_log_partial.size() << "): " << previous_log_partial << std::endl;

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

                            std::cout << "Finished replaying undelivered log." << std::endl;
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
                            watch.file_offset = 0;
                            watch.saveOffset( this->db );

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






    string InotifyWatcher::filterLogLine( const string& unfiltered_log_line ) const{

        string filtered_log_line = unfiltered_log_line;


        //get timestamp (nanoseconds)
            string current_time_string = "0.0";
            timespec current_time;
            if( clock_gettime(CLOCK_REALTIME, &current_time) == 0 ){

                char buffer[50];

                // thanks to https://stackoverflow.com/a/8304728/278976
                sprintf( buffer, "%lld.%.9ld", (long long)current_time.tv_sec, current_time.tv_nsec );
                current_time_string = string(buffer);

            }


        // add your pre-filtering code here
        /*
        if( 0 ){
            filtered_log_line = "{\"log_type\":\"tombstone\"}";
            return filtered_log_line;
        }
        */


        size_t log_length = filtered_log_line.size();

        if( log_length == 0 ){
            return filtered_log_line;
        }


        string json_meta = "{\"@timestamp\":" + current_time_string + ",\"host\":\"" + this->watch.hostname + "\",\"source\":\"" + this->watch.watched_filepath + "\",\"prd\":\"" + this->watch.product_code + "\"";


        //unstructured single-line log entry
            if( filtered_log_line[0] != '{' ){

                filtered_log_line = json_meta + ",\"log\":\"" + escape_to_json_string(filtered_log_line) + "\"}";
                return filtered_log_line;

            }



        //embedded single-line JSON
            if( filtered_log_line[0] == '{' && log_length >= 4 ){

                //this embedded single-line JSON MUST begin and end with a brace

                string removed_braces = filtered_log_line.substr( 1, log_length - 2 );

                filtered_log_line = json_meta + ",\"log_obj\":" + removed_braces + "}";
                return filtered_log_line;

            }


        return filtered_log_line;

    }



}
