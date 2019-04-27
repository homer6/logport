cmake_minimum_required( VERSION "2.6.4" )

project( logport CXX C )


include_directories(
    include
    # /usr/local/include/librdkafka
)

set( LOGPORT_SOURCES
    src/Database.cc
    src/Common.cc
    src/Platform.cc
    src/Observer.cc
    src/Inspector.cc
    src/PreparedStatement.cc
    src/LogPort.cc
    src/Watch.cc
    src/KafkaProducer.cc
    src/InotifyWatcher.cc
    src/LevelTriggeredEpollWatcher.cc
    src/sqlite3.c
)

set( LOGPORT_LIBS
    rdkafka
    pthread
    dl
)


# LogPort is built with c++98 to support OEL5.11

set( LOGPORT_COMPILE_OPTIONS
    -Wall
    -Wextra
    -O3   
    -std=c++98
    #-std=gnu++11
    #-std=c++1y
    #-std=c++17
)

add_executable( build/logport ${LOGPORT_SOURCES} src/logport_main.cc )

target_link_libraries( build/logport -Wl,-rpath=/usr/local/lib/logport:. ${LOGPORT_LIBS} )

set_target_properties( build/logport
    PROPERTIES
    COMPILE_OPTIONS  "${LOGPORT_COMPILE_OPTIONS}"
)

