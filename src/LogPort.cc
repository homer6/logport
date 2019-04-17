#include "LogPort.h"
#include "Watch.h"

#include <iostream>
using std::cout;
using std::endl;

#include <unistd.h>
#include <signal.h>


static logport::LogPort* logport_app_ptr;

static void signal_handler_stop( int /*sig*/ ){
    logport_app_ptr->run = false;
    cout << "stopping logport" << endl;
}


namespace logport{

	LogPort::LogPort()
		:run(true)
	{


	}

	void LogPort::registerSignalHandlers(){

		logport_app_ptr = this;

        // Signal handler for clean shutdown 
        signal( SIGINT, signal_handler_stop );

	}


	void LogPort::install(){}
	void LogPort::uninstall(){}

	void LogPort::start(){}
	void LogPort::stop(){}
	void LogPort::restart(){}
	void LogPort::reload(){}
	void LogPort::status(){}


    void LogPort::printHelp(){

		const char *help_message = 
"usage: logport [--version] [--help] <command> [<args>]\n"
"\n"
"These are common logport commands used in various situations:\n"
"\n"
"add system service\n"
"   install    Installs logport as a system service (and enables it)\n"
"   uninstall  Removes logport service and configuration\n"
"   enable     Enables the service to start on bootup\n"
"   disable    Disables the service from starting on bootup\n"
"\n"
"systemd commands\n"
"   start      Starts the service\n"
"   stop       Stops the service\n"
"   restart    Restarts the service gracefully\n"
"   status     Prints the running status of logport\n"
"   reload     Explicitly reloads the configuration file\n"
"\n"
"manage watches\n"
"   watch      Add a watch (will also implicitly install logport)\n"
"   unwatch    Remove a watch\n"
"   watches    List all watches\n"
"\n"
"manage settings\n"
"   set        Set a setting's value\n"
"   unset      Clear a setting's value\n"
"\n"
"Please see: https://github.com/homer6/logport to report issues \n"
"or view documentation.\n";

		cout << help_message << endl;

    }



    void LogPort::printVersion(){}




    int LogPort::runFromCommandLine( int argc, char **argv ){

    	this->printHelp();

    	return 0;

    }




	void LogPort::addWatch( const Watch& watch ){}


}