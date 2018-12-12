#ifndef LOGPORT_INOTIFY_WATCHER_H
#define LOGPORT_INOTIFY_WATCHER_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "KafkaProducer.h"

class InotifyWatcher{

    public:
        InotifyWatcher( const string& watched_file, KafkaProducer &kafka_producer );
        ~InotifyWatcher();

        void watch(); //throws on failure

        int run;

    protected:
        string watched_file;
        KafkaProducer &kafka_producer;

        int inotify_fd;
        int inotify_watch_descriptor;
};


#endif //LOGPORT_INOTIFY_WATCHER_H
