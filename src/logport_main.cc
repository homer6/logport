#include <string.h>

#include <stdexcept>
#include <iostream>

using std::cout;
using std::endl;
using std::cerr;



#include "LogPort.h"


int main( int argc, char **argv ){

    logport::LogPort logport_app;
    logport_app.registerSignalHandlers();
    return logport_app.runFromCommandLine( argc, argv );

}

