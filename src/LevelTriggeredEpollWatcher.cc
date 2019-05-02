#include "LevelTriggeredEpollWatcher.h"

#include <stdexcept>

#include <sys/epoll.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <errno.h>

#include <string>
using std::string;

#define MAX_EVENTS 10


namespace logport{

    LevelTriggeredEpollWatcher::LevelTriggeredEpollWatcher( int watching_file_descriptor )
        :watching_file_descriptor(watching_file_descriptor)
    {

        char error_string_buffer[1024];

        struct epoll_event ev;

        int epoll_result;

        this->epollfd = epoll_create(1);
        if( this->epollfd == -1 ){
            snprintf( error_string_buffer, sizeof(error_string_buffer), "%d", this->epollfd );
            throw std::runtime_error( "Failed to create epoll fd: " + string(error_string_buffer) );
        }

        ev.events = EPOLLIN;
        ev.data.fd = this->watching_file_descriptor;

        epoll_result = epoll_ctl( this->epollfd, EPOLL_CTL_ADD, this->watching_file_descriptor, &ev );

        if( epoll_result == -1 ){
            close( this->epollfd );
            snprintf( error_string_buffer, sizeof(error_string_buffer), "%d %d", epoll_result, this->watching_file_descriptor );
            throw std::runtime_error( "Failed to add epoll watched file: " + string(error_string_buffer) );
        }

    }


    LevelTriggeredEpollWatcher::~LevelTriggeredEpollWatcher(){

        close( this->epollfd );

    }




    bool LevelTriggeredEpollWatcher::watch( int timeout_ms ){

        struct epoll_event events[MAX_EVENTS];
        int number_of_fds;

        char error_string_buffer[1024];

        number_of_fds = epoll_wait( this->epollfd, events, MAX_EVENTS, timeout_ms );
        if( number_of_fds == -1 && errno != EINTR ){
            snprintf( error_string_buffer, sizeof(error_string_buffer), "%d", errno );
            throw std::runtime_error( "Failed on epoll_wait. errno: " + string(error_string_buffer) );
        }

        for( int n = 0; n < number_of_fds; ++n ){

            if( events[n].data.fd == this->watching_file_descriptor ){
                return true;
            }

        }

        return false;

    }



}
