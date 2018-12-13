#ifndef LOGPORT_LEVEL_TRIGGERED_EPOLL_WATCHER_H
#define LOGPORT_LEVEL_TRIGGERED_EPOLL_WATCHER_H

class LevelTriggeredEpollWatcher{

    public:
        LevelTriggeredEpollWatcher( int watching_file_descriptor );
        ~LevelTriggeredEpollWatcher();

        bool watch( int timeout_ms = 1000 ); //throws on failure

    protected:
    	int watching_file_descriptor;
    	int epollfd;
        
};


#endif //LOGPORT_LEVEL_TRIGGERED_EPOLL_WATCHER_H
