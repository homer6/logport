#include <string.h>

#include <stdexcept>

#include <iostream>
using std::cout;
using std::endl;
using std::cerr;

#include <string>
using std::string;


#include "LogPort.h"


int main( int argc, char **argv ){

    try{

        logport::LogPort logport_app;
        logport_app.registerSignalHandlers();
        return logport_app.runFromCommandLine( argc, argv );

    }catch( std::exception& e ){

        logport::Observer observer;
        string exception_message( "Fatal exception caught: " );
        exception_message += string( e.what() );
        observer.addLogEntry( exception_message );
        cerr << exception_message << endl;
        return -1;

    }catch( ... ){

        logport::Observer observer;
        string exception_message( "Unknown fatal exception caught." );
        observer.addLogEntry( exception_message );
        cerr << exception_message << endl;
        return -1;

    }

}

