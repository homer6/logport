#ifndef LOGPORT_INOTIFY_WATCHER_H
#define LOGPORT_INOTIFY_WATCHER_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "KafkaProducer.h"

namespace logport{

    class Database;
    class Watch;

    class InotifyWatcher{

        public:
            InotifyWatcher( Database& db, const string& watched_file, const string &undelivered_log, KafkaProducer &kafka_producer );
            ~InotifyWatcher();

            void watch( Watch watch ); //throws on failure

            string filterLogLine( const string& unfiltered_log_line ) const;

            string escapeToJsonString( const string& unescaped_string ) const;

        protected:
            Database& db;

        public:
            int run;

        protected:
            string watched_file;

            string undelivered_log;
            int undelivered_log_fd;

            KafkaProducer &kafka_producer;

            int inotify_fd;
            int inotify_watch_descriptor;

            

    };

}

#endif //LOGPORT_INOTIFY_WATCHER_H
