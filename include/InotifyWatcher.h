#ifndef LOGPORT_INOTIFY_WATCHER_H
#define LOGPORT_INOTIFY_WATCHER_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "KafkaProducer.h"

#include <fstream>


namespace logport{

    class Database;
    class Watch;
    class Observer;

    class InotifyWatcher{

        public:
            InotifyWatcher( Database& db, KafkaProducer &kafka_producer, Watch& watch, Observer& observer );
            ~InotifyWatcher();

            void startWatching(); //throws on failure

            string filterLogLine( const string& unfiltered_log_line ) const;

            string escapeToJsonString( const string& unescaped_string ) const;

        protected:
            Database& db;

        public:
            bool run;

        protected:
            string watched_file;

            string undelivered_log;
            int undelivered_log_fd;

            KafkaProducer &kafka_producer;

            int inotify_fd;
            int inotify_watch_descriptor;

            Watch& watch;
            Observer& observer;

    };

}

#endif //LOGPORT_INOTIFY_WATCHER_H
