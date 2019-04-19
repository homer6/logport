#include "KafkaProducer.h"



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


/* Typical include path would be <librdkafka/rdkafka.h>, but this program
 * is builtin from within the librdkafka source tree and thus differs. */
//#include "rdkafka.h"
#include <librdkafka/rdkafka.h>

#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <errno.h>

#include <stdexcept>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace logport{


    static int undelivered_log_fd_static;

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

        if( rkmessage->err ){

            fprintf( stderr, "Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err) );

            if( undelivered_log_fd_static != -1 ){

                int result_bytes = write( undelivered_log_fd_static, rkmessage->payload, rkmessage->len );

                if( result_bytes < 0 ){
                    fprintf( stderr, "Failed to write to undelivered_log. errno: %d\n", errno );
                }else{
                    if( (size_t)result_bytes != rkmessage->len ){
                        fprintf( stderr, "Write mismatch in undelivered_log. %lu bytes expected but only %lu written.\n", rkmessage->len, (size_t)result_bytes );
                    }
                }



                //append newline
                    char newline_buffer[10];
                    newline_buffer[0] = '\n';

                    result_bytes = write( undelivered_log_fd_static, newline_buffer, 1 );
                    if( result_bytes < 0 ){
                        fprintf( stderr, "Failed to write to undelivered_log newline. errno: %d\n", errno );
                    }
                

            }else{

                fprintf( stderr, "FAILED TO RECORD UNDELIVERED MESSAGES\n" );

            }

        }else{

            fprintf( stderr, "Message delivered (%zd bytes, partition %d)\n", rkmessage->len, rkmessage->partition );

        }

        /* The rkmessage is destroyed automatically by librdkafka */

    }





    KafkaProducer::KafkaProducer( const string &brokers_list, const string &topic, const string &undelivered_log )
            :brokers_list(brokers_list), topic(topic), undelivered_log(undelivered_log), undelivered_log_open(false)
    {

        undelivered_log_fd_static = -1;

        /*
         * Create Kafka client configuration place-holder
         */
        rd_kafka_conf_t *conf;      /* Temporary configuration object */ 
        conf = rd_kafka_conf_new();

        /* Set bootstrap broker(s) as a comma-separated list of
         * host or host:port (default port 9092).
         * librdkafka will use the bootstrap brokers to acquire the full
         * set of brokers from the cluster. */
        if( rd_kafka_conf_set(conf, "bootstrap.servers", brokers_list.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK ){
            rd_kafka_conf_destroy(conf);
            throw std::runtime_error( string("KafkaProducer: Failed to set configuration for bootstrap.servers: ") + errstr );
        }

        const string message_timeout_ms = "10000"; //10 seconds; this must be shorter than the timeout for rd_kafka_flush below (in the deconstructor) or messages will be lost and not recorded in the undelivered_log
        if( rd_kafka_conf_set(conf, "message.timeout.ms", message_timeout_ms.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK ){
            rd_kafka_conf_destroy(conf);
            throw std::runtime_error( string("KafkaProducer: Failed to set configuration for message.timeout.ms: ") + errstr );
        }

        /*
        const string socket_max_fails = "100";
        if( rd_kafka_conf_set(conf, "socket.max.fails", socket_max_fails.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK ){
            rd_kafka_conf_destroy(conf);
            throw std::runtime_error( string("KafkaProducer: Failed to set configuration for socket.max.fails: ") + errstr );
        }
        */

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
        this->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if( !this->rk ){
            //fprintf( stderr, "%% Failed to create new producer: %s\n", errstr );
            throw std::runtime_error( string("KafkaProducer: Failed to create new producer: ") + rd_kafka_err2str(rd_kafka_last_error()) );
        }


        /* Create topic object that will be reused for each message
         * produced.
         *
         * Both the producer instance (rd_kafka_t) and topic objects (topic_t)
         * are long-lived objects that should be reused as much as possible.
         */
        this->rkt = rd_kafka_topic_new( this->rk, topic.c_str(), NULL );
        if( !this->rkt ){
            //fprintf( stderr, "%% Failed to create topic object: %s\n", rd_kafka_err2str(rd_kafka_last_error()) );
            rd_kafka_destroy(this->rk);
            throw std::runtime_error( string("KafkaProducer: Failed to create topic object: ") + rd_kafka_err2str(rd_kafka_last_error()) );
        }


    }



    KafkaProducer::~KafkaProducer(){

        /* Wait for final messages to be delivered or fail.
         * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
         * waits for all messages to be delivered. */
        //fprintf(stderr, "%% Flushing final messages..\n");
        rd_kafka_flush(this->rk, 15 * 1000 /* wait for max 15 seconds */);
        //this wait must be longer than the message.timeout.ms in the conf above or the messages will be lost and not stored in the undelivered_log

        /* Destroy topic object */
        rd_kafka_topic_destroy(this->rkt);

        /* Destroy the producer instance */
        rd_kafka_destroy(this->rk);


        if( this->undelivered_log_open ){
            undelivered_log_fd_static = -1;
            close( this->undelivered_log_fd );
        }
        

    }



    void KafkaProducer::produce( const string& message ){


        /**
         * @brief Produce and send a single message to broker.
         *
         * \p rkt is the target topic which must have been previously created with
         * `rd_kafka_topic_new()`.
         *
         * `rd_kafka_produce()` is an asynch non-blocking API.
         * See `rd_kafka_conf_set_dr_msg_cb` on how to setup a callback to be called
         * once the delivery status (success or failure) is known. The delivery report
         * is trigged by the application calling `rd_kafka_poll()` (at regular
         * intervals) or `rd_kafka_flush()` (at termination). 
         *
         * Since producing is asynchronous, you should call `rd_kafka_flush()` before
         * you destroy the producer. Otherwise, any outstanding messages will be
         * silently discarded.
         *
         * When temporary errors occur, librdkafka automatically retries to produce the
         * messages. Retries are triggered after retry.backoff.ms and when the
         * leader broker for the given partition is available. Otherwise, librdkafka
         * falls back to polling the topic metadata to monitor when a new leader is
         * elected (see the topic.metadata.refresh.fast.interval.ms and
         * topic.metadata.refresh.interval.ms configurations) and then performs a
         * retry. A delivery error will occur if the message could not be produced
         * within message.timeout.ms.
         *
         * See the "Message reliability" chapter in INTRODUCTION.md for more
         * information.
         *
         * \p partition is the target partition, either:
         *   - RD_KAFKA_PARTITION_UA (unassigned) for
         *     automatic partitioning using the topic's partitioner function, or
         *   - a fixed partition (0..N)
         *
         * \p msgflags is zero or more of the following flags OR:ed together:
         *    RD_KAFKA_MSG_F_BLOCK - block \p produce*() call if
         *                           \p queue.buffering.max.messages or
         *                           \p queue.buffering.max.kbytes are exceeded.
         *                           Messages are considered in-queue from the point they
         *                           are accepted by produce() until their corresponding
         *                           delivery report callback/event returns.
         *                           It is thus a requirement to call 
         *                           rd_kafka_poll() (or equiv.) from a separate
         *                           thread when F_BLOCK is used.
         *                           See WARNING on \c RD_KAFKA_MSG_F_BLOCK above.
         *
         *    RD_KAFKA_MSG_F_FREE - rdkafka will free(3) \p payload when it is done
         *                          with it.
         *    RD_KAFKA_MSG_F_COPY - the \p payload data will be copied and the 
         *                          \p payload pointer will not be used by rdkafka
         *                          after the call returns.
         *    RD_KAFKA_MSG_F_PARTITION - produce_batch() will honour per-message
         *                               partition, either set manually or by the
         *                               configured partitioner.
         *
         *    .._F_FREE and .._F_COPY are mutually exclusive.
         *
         *    If the function returns -1 and RD_KAFKA_MSG_F_FREE was specified, then
         *    the memory associated with the payload is still the caller's
         *    responsibility.
         *
         * \p payload is the message payload of size \p len bytes.
         *
         * \p key is an optional message key of size \p keylen bytes, if non-NULL it
         * will be passed to the topic partitioner as well as be sent with the
         * message to the broker and passed on to the consumer.
         *
         * \p msg_opaque is an optional application-provided per-message opaque
         * pointer that will provided in the delivery report callback (`dr_cb`) for
         * referencing this message.
         *
         * @remark on_send() and on_acknowledgement() interceptors may be called
         *         from this function. on_acknowledgement() will only be called if the
         *         message fails partitioning.
         *
         * @returns 0 on success or -1 on error in which case errno is set accordingly:
         *  - ENOBUFS  - maximum number of outstanding messages has been reached:
         *               "queue.buffering.max.messages"
         *               (RD_KAFKA_RESP_ERR__QUEUE_FULL)
         *  - EMSGSIZE - message is larger than configured max size:
         *               "messages.max.bytes".
         *               (RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE)
         *  - ESRCH    - requested \p partition is unknown in the Kafka cluster.
         *               (RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION)
         *  - ENOENT   - topic is unknown in the Kafka cluster.
         *               (RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
         *  - ECANCELED - fatal error has been raised on producer, see
         *                rd_kafka_fatal_error().
         *
         * @sa Use rd_kafka_errno2err() to convert `errno` to rdkafka error code.
         */






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

        if( rd_kafka_produce(
                    /* Topic object */
                    this->rkt,
                    /* Use builtin partitioner to select partition*/
                    RD_KAFKA_PARTITION_UA,
                    /* Make a copy of the payload. */
                    RD_KAFKA_MSG_F_COPY,
                    /* Message payload (value) and length */
                    const_cast<void*>( static_cast<const void*>(message.c_str()) ), message.size(),
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
                        rd_kafka_topic_name(this->rkt),
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

                        this->poll( 1000 ); //block for max 1000ms

                        goto retry;
                }

        } else {

            //successfully queued message

            //fprintf(stderr, "%% Enqueued message (%zd bytes) for topic %s\n", len, rd_kafka_topic_name(rkt) );

        }


    }



    void KafkaProducer::poll( int timeout_ms ){

        rd_kafka_poll( this->rk, timeout_ms );

    }



    void KafkaProducer::openUndeliveredLog(){

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

            rd_kafka_topic_destroy(this->rkt);
            rd_kafka_destroy(this->rk);

            snprintf(error_string_buffer, sizeof(error_string_buffer), "%d", errno);
            throw std::runtime_error( "Failed to open undelivered log file for writing: errno " + string(error_string_buffer) );
        }

        undelivered_log_fd_static = this->undelivered_log_fd;

        this->undelivered_log_open = true;

    }

}
