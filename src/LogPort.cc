#include "LogPort.h"
#include "Watch.h"

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <unistd.h>
#include <signal.h>

#include "InotifyWatcher.h"
#include "KafkaProducer.h"





static logport::LogPort* logport_app_ptr;

static void signal_handler_stop( int /*sig*/ ){
    logport_app_ptr->run = false;
    cout << "stopping logport" << endl;
    //exit(0);
}


namespace logport{

	LogPort::LogPort()
		:run(true), current_version("0.1.0")
	{


	}

	void LogPort::registerSignalHandlers(){

		logport_app_ptr = this;

        // Signal handler for clean shutdown 
        //signal( SIGINT, signal_handler_stop );

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



    void LogPort::printVersion(){

    	cout << "logport version " << this->current_version << endl;

    }




    int LogPort::runFromCommandLine( int argc, char **argv ){

    	int x = 0;

    	if( argc <= 1 ){
    		this->printHelp();
    		return 0;
    	}

    	while( x < argc ){
			this->command_line_arguments.push_back( string(argv[x]) );
			x++;
    	}

    	if( argc > 1 ){
    		this->command = this->command_line_arguments[1];
    	}

    	if( this->command == "-h" || this->command == "--help" || this->command == "help" ){
    		this->printHelp();
    		return 0;
    	}

    	if( this->command == "-v" || this->command == "--version" || this->command == "version" ){
    		this->printVersion();
    		return 0;
    	}

    	for( x = 0; x < argc; x++ ){
			cout << this->command_line_arguments[x] << endl;   		
    	}

    	if( this->command == "watch" ){

    		Watch watch;
    		watch.brokers = this->getDefaultBrokers();
    		watch.topic = this->getDefaultTopic();

    		if( argc > 2 ){
    			watch.watched_filepath = this->command_line_arguments[2];
    		}else{
    			cerr << "Watch requires a file to watch." << endl;
    			return -1;
    		}

    		if( argc > 3 ){
    			watch.topic = this->command_line_arguments[3];
    		}

    		if( argc > 4 ){
    			watch.brokers = this->command_line_arguments[4];
    		}

    		watch.undelivered_log_filepath = watch.watched_filepath + "_undelivered";

    		this->addWatch( watch );

    	}

    	return 0;

    }


	string LogPort::getDefaultTopic(){
		return "default_topic";
	}

	string LogPort::getDefaultBrokers(){
		return "localhost:9092";
	}


	void LogPort::addWatch( const Watch& watch ){

		KafkaProducer kafka_producer( watch.brokers, watch.topic, watch.undelivered_log_filepath );  

		InotifyWatcher watcher( watch.watched_filepath, watch.undelivered_log_filepath, kafka_producer );  //expects undelivered log to exist
		//inotify_watcher_ptr = &watcher;

		watcher.watch(); //main loop; blocks

	}


}
