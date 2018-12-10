
/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2017, Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Simple Apache Kafka producer
 * using the Kafka driver from librdkafka
 * (https://github.com/edenhill/librdkafka)
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>


/* Typical include path would be <librdkafka/rdkafka.h>, but this program
 * is builtin from within the librdkafka source tree and thus differs. */
//#include "rdkafka.h"
#include <librdkafka/rdkafka.h>

#include <sys/inotify.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

static int run = 1;

/**
 * @brief Signal termination of program
 */
static void stop (int /*sig*/) {
        run = 0;
        fclose(stdin); /* abort fgets() */
}


/**
 * @brief Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if
 * the message was succesfully delivered
 * (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or permanently
 * failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on
 * the application's thread.
 */
static void dr_msg_cb( rd_kafka_t */*rk*/, const rd_kafka_message_t *rkmessage, void */*opaque*/ ){

        if (rkmessage->err)
        	fprintf( stderr, "%% Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err) );
        else
            fprintf( stderr, "%% Message delivered (%zd bytes, partition %d)\n", rkmessage->len, rkmessage->partition );

        /* The rkmessage is destroyed automatically by librdkafka */
}


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



int main (int argc, char **argv) {
        rd_kafka_t *rk;             /* Producer instance handle */
        rd_kafka_topic_t *rkt;      /* Topic object */
        rd_kafka_conf_t *conf;      /* Temporary configuration object */
        char errstr[512];           /* librdkafka API error reporting buffer */
        char buf[512];              /* Message value temporary buffer */
        const char *brokers;        /* Argument: broker list */
        const char *topic;          /* Argument: topic to produce to */
        const char *file_to_watch;  /* Argument: log file that will be watched */

        /*
         * Argument validation
         */
        if (argc != 4) {
                fprintf(stderr, "%% Usage: %s <broker> <topic> <file-to-watch>\n", argv[0]);
                return 1;
        }

        brokers       = argv[1];
        topic         = argv[2];
        file_to_watch = argv[3];


        /*
         * Create Kafka client configuration place-holder
         */
        conf = rd_kafka_conf_new();

        /* Set bootstrap broker(s) as a comma-separated list of
         * host or host:port (default port 9092).
         * librdkafka will use the bootstrap brokers to acquire the full
         * set of brokers from the cluster. */
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                return 1;
        }

        /* Set the delivery report callback.
         * This callback will be called once per message to inform
         * the application if delivery succeeded or failed.
         * See dr_msg_cb() above. */
        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);


        /*
         * Create producer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if (!rk) {
                fprintf(stderr,
                        "%% Failed to create new producer: %s\n", errstr);
                return 1;
        }


        /* Create topic object that will be reused for each message
         * produced.
         *
         * Both the producer instance (rd_kafka_t) and topic objects (topic_t)
         * are long-lived objects that should be reused as much as possible.
         */
        rkt = rd_kafka_topic_new(rk, topic, NULL);
        if (!rkt) {
                fprintf(stderr, "%% Failed to create topic object: %s\n",
                        rd_kafka_err2str(rd_kafka_last_error()));
                rd_kafka_destroy(rk);
                return 1;
        }

        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);




       	/* Create inotify instance; add watch descriptors */

       	int inotify_fd = inotify_init();   /* Create inotify instance */
    	if( inotify_fd == -1 ){
    		fprintf(stderr, "%% Failed to create inotify instance: errno %d\n", errno );
    		return 1;
    	}
        	

        int inotify_watch_descriptor;
        if( (inotify_watch_descriptor = inotify_add_watch(inotify_fd, file_to_watch, IN_ALL_EVENTS)) == -1 ){
            fprintf(stderr, "%% Failed to add inotify watch descriptor: errno %d\n", errno );
            return 1;
        }

		char inotify_event_buffer[INOTIFY_EVENT_BUFFER_LENGTH] __attribute__ ((aligned(8)));
		ssize_t numRead;
		char *p;
		struct inotify_event *in_event;


		/*
        fprintf(stderr,
                "%% Type some text and hit enter to produce message\n"
                "%% Or just hit enter to only serve delivery reports\n"
                "%% Press Ctrl-C or Ctrl-D to exit\n");
        */

        while( run ){

				numRead = read(inotify_fd, inotify_event_buffer, INOTIFY_EVENT_BUFFER_LENGTH);
				if( numRead == 0 ){
 					fprintf( stderr, "%% read() from inotify fd returned 0!" );
            		return 1;
				} 

				if( numRead == -1 ){
					fprintf( stderr, "%% read() errno %d", errno );
					return 1;
				}

				printf("Read %ld bytes from inotify fd\n", (long) numRead);

				/* Process all of the events in buffer returned by read() */

				for( p = inotify_event_buffer; p < inotify_event_buffer + numRead; ){
				    in_event = (struct inotify_event *) p;
				    displayInotifyEvent(in_event);

				    p += sizeof(struct inotify_event) + in_event->len;
				}

                size_t len = strlen(buf);

                if (buf[len-1] == '\n') /* Remove newline */
                        buf[--len] = '\0';

                if (len == 0) {
	                /* Empty line: only serve delivery reports */
					rd_kafka_poll(rk, 0/*non-blocking */);
	                continue; 
                }

                continue;

                /*
                 * Send/Produce message.
                 * This is an asynchronous call, on success it will only
                 * enqueue the message on the internal producer queue.
                 * The actual delivery attempts to the broker are handled
                 * by background threads.
                 * The previously registered delivery report callback
                 * (dr_msg_cb) is used to signal back to the application
                 * when the message has been delivered (or failed).
                 */
        retry:
                if (rd_kafka_produce(
                            /* Topic object */
                            rkt,
                            /* Use builtin partitioner to select partition*/
                            RD_KAFKA_PARTITION_UA,
                            /* Make a copy of the payload. */
                            RD_KAFKA_MSG_F_COPY,
                            /* Message payload (value) and length */
                            buf, len,
                            /* Optional key and its length */
                            NULL, 0,
                            /* Message opaque, provided in
                             * delivery report callback as
                             * msg_opaque. */
                            NULL) == -1) {
                        /**
                         * Failed to *enqueue* message for producing.
                         */
                        fprintf(stderr,
                                "%% Failed to produce to topic %s: %s\n",
                                rd_kafka_topic_name(rkt),
                                rd_kafka_err2str(rd_kafka_last_error()));

                        /* Poll to handle delivery reports */
                        if (rd_kafka_last_error() ==
                            RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                                /* If the internal queue is full, wait for
                                 * messages to be delivered and then retry.
                                 * The internal queue represents both
                                 * messages to be sent and messages that have
                                 * been sent or failed, awaiting their
                                 * delivery report callback to be called.
                                 *
                                 * The internal queue is limited by the
                                 * configuration property
                                 * queue.buffering.max.messages */
                                rd_kafka_poll(rk, 1000/*block for max 1000ms*/);
                                goto retry;
                        }
                } else {
                        fprintf(stderr, "%% Enqueued message (%zd bytes) "
                                "for topic %s\n",
                                len, rd_kafka_topic_name(rkt));
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
                rd_kafka_poll(rk, 0/*non-blocking*/);
        }


        /* Wait for final messages to be delivered or fail.
         * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
         * waits for all messages to be delivered. */
        fprintf(stderr, "%% Flushing final messages..\n");
        rd_kafka_flush(rk, 10*1000 /* wait for max 10 seconds */);

        /* Destroy topic object */
        rd_kafka_topic_destroy(rkt);

        /* Destroy the producer instance */
        rd_kafka_destroy(rk);

        return 0;
}




/*
#include "LogPort.h"

#include <vector>
using std::vector;

#include <string>
using std::string;



int main(){

	// LogPort is built with c++98 to support OEL5.11

	vector<string> watched_files;
	watched_files.push_back( "/var/log/syslog" );


    LogPort log_port( "127.0.0.1:9092", watched_files );

    log_port.startWatching();

    return 0;

}
*/