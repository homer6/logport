cmake_minimum_required( VERSION "2.6.4" )

project( logport CXX C )


include_directories(
    include
)


set( LOGPORT_SOURCES
    src/LogPort.cc
)

add_executable( build/logport ${LOGPORT_SOURCES} src/logport_main.cc )