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
#include "Database.h"
#include "PreparedStatement.h"
#include "sqlite3.h"


#include <cstdio>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <fstream>

#include <sys/stat.h>


static logport::LogPort* logport_app_ptr;

static void signal_handler_stop( int /*sig*/ ){
    logport_app_ptr->run = false;
    cout << "stopping logport" << endl;
    //exit(0);
}


namespace logport{

	LogPort::LogPort()
		:db(NULL), run(true), current_version("0.1.0")
	{


	}

	LogPort::~LogPort(){

		if( this->db != NULL ){
			delete this->db;
		}

	}





	void LogPort::registerSignalHandlers(){

		logport_app_ptr = this;

        // Signal handler for clean shutdown 
        //signal( SIGINT, signal_handler_stop );

	}


	void LogPort::install(){

		this->executeCommand( "mkdir -p /usr/local/lib/logport" );
		this->executeCommand( "cp logport /usr/local/bin" );

		//create the db
			this->executeCommand( "mkdir -p /usr/local/logport" );
			this->executeCommand( "chmod 777 /usr/local/logport" );

			bool database_exists = this->fileExists("/usr/local/logport/logport.db"); 
			
			{
				Database db; //creates the db
				if( !database_exists ){
					db.createDatabase();
				}
			}//explicitly closes the db so we can chmod it

			this->executeCommand( "chmod ugo+w /usr/local/logport/logport.db" );
		
		this->installInitScript();
		this->executeCommand( "cp librdkafka.so.1 /usr/local/lib/logport" );
		this->executeCommand( "systemctl start logport" );
		this->executeCommand( "systemctl enable logport" );
		
		cout << "Logport installed as a system service, started, and enabled on bootup." << endl;
		cout << "The logport binary is now in your path." << endl;
		cout << "Run these commands to remove the downloaded files. This will not remove logport from your system." << endl;

	}


	void LogPort::uninstall(){

		this->executeCommand( "systemctl stop logport" );
		this->executeCommand( "systemctl disable logport" );
		this->executeCommand( "rm /etc/init.d/logport" );

		this->executeCommand( "rm /usr/local/logport/logport.db" );
		this->executeCommand( "rmdir /usr/local/logport" );

		cout << "Run these commands to finalize the uninstall:" << endl;
		cout << "rm /usr/local/lib/logport/librdkafka.so.1" << endl;
		cout << "rmdir /usr/local/lib/logport" << endl;
		cout << "rm /usr/local/bin/logport" << endl;

	}


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


	void LogPort::printHelpWatch(){

		cerr << "Usage: logport watch [OPTION]... [FILE]...\n"
				"Adds one or more files to be watched.\n"
				"\n"
				"Mandatory arguments to long options are mandatory for short options too.\n"
				"  -b, --brokers [BROKERS]    a csv list of kafka brokers (optional: defaults to brokers default)\n"
				"  -t, --topic [TOPIC]        a destination kafka topic (optional: defaults to topic default)"
		<< endl;

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

    	//for( x = 0; x < argc; x++ ){
		//	cout << this->command_line_arguments[x] << endl;   		
    	//}

    	if( this->command == "watch" ){

    		if( argc <= 2 ){
    			this->printHelpWatch();
    			return -1;
    		}

    		int current_argument_offset = 2;

    		string this_brokers = this->getDefaultBrokers();
    		string this_topic = this->getDefaultTopic();

    		while( current_argument_offset < argc ){

    			string current_argument = this->command_line_arguments[ current_argument_offset ];


    			if( current_argument == "--topic" || current_argument == "--topics" || current_argument == "-t" ){

    				current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				this_topic = this->command_line_arguments[ current_argument_offset ];

					current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				continue;

    			}


    			if( current_argument == "--brokers" || current_argument == "--broker" || current_argument == "-b" ){

    				current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				this_brokers = this->command_line_arguments[ current_argument_offset ];

					current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				continue;

    			}

	    		Watch watch;
	    		watch.brokers = this_brokers;
	    		watch.topic = this_topic;
	    		watch.watched_filepath = current_argument;
	    		watch.undelivered_log_filepath = watch.watched_filepath + "_undelivered";

	    		this->addWatch( watch );

    			current_argument_offset++;

    		}

    		return 0;

    	}

    	if( this->command == "watches" ){
    		this->listWatches();
    		return 0;
    	}

    	if( this->command == "install" ){
    		this->install();
    		return 0;
    	}

    	if( this->command == "uninstall" ){
    		this->uninstall();
    		return 0;
    	}


    	this->printHelp();

    	return 1;

    }


	string LogPort::getDefaultTopic(){
		return "default_topic";
	}

	string LogPort::getDefaultBrokers(){
		return "localhost:9092";
	}


	void LogPort::listWatches(){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "select * from watches;" );

		while( statement.step() == SQLITE_ROW ){

			Watch watch(statement);
			
			cout << watch.id << " " << watch.watched_filepath << " " << watch.brokers << " " << watch.topic << " " << watch.file_offset << endl;

		}

	}


	void LogPort::addWatch( const Watch& watch ){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "INSERT INTO watches ( filepath, file_offset, brokers, topic ) VALUES ( ?, ?, ?, ? );" );

		watch.bind( statement, true );

		statement.step();
		statement.reset();
		statement.clearBindings();


		/*
		KafkaProducer kafka_producer( watch.brokers, watch.topic, watch.undelivered_log_filepath );  

		InotifyWatcher watcher( watch.watched_filepath, watch.undelivered_log_filepath, kafka_producer );  //expects undelivered log to exist
		//inotify_watcher_ptr = &watcher;

		watcher.watch(); //main loop; blocks
		*/

	}


	string LogPort::executeCommand( const string& command ){

		// http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
		char buffer[4096];

		string output;

		FILE* pipe = popen( command.c_str(), "r" );
		if( !pipe ){
			throw std::runtime_error( "popen() failed" );
		}

		try {
			while( fgets(buffer, 4096, pipe) != NULL ){
				output += buffer;
			}
		}catch(...){
			pclose( pipe );
			throw;
		}
		pclose( pipe );

		return output;

	}


	bool LogPort::fileExists( const string& filename ){
		//https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
		struct stat buffer;
		return ( stat(filename.c_str(), &buffer) == 0 ); 
	}




	void LogPort::installInitScript(){

		const char *init_d_file_contents = 
"#!/bin/bash\n"
"#\n"
"#       /etc/init.d/logport\n"
"#\n"
"#       See: https://github.com/homer6/logport\n"
"#\n"
"# chkconfig: 345 99 01\n"
"# description: logport sends log data to kafka\n"
"# processname: logport\n"
"### BEGIN INIT INFO\n"
"# Provides:          logport\n"
"# Required-Start:    $all\n"
"# Required-Stop:\n"
"# Default-Start:     2 3 4 5\n"
"# Default-Stop:\n"
"# Short-Description: logport sends log data to kafka\n"
"### END INIT INFO\n"
"\n"
"RETVAL=0\n"
"prog=\"logport\"\n"
"\n"
"LOGPORT_EXECUTABLE=/usr/local/bin/logport\n"
"\n"
"start() {\n"
"        $LOGPORT_EXECUTABLE start\n"
"        RETVAL=$?\n"
"        return $RETVAL\n"
"}\n"
"\n"
"stop() {\n"
"        $LOGPORT_EXECUTABLE stop\n"
"        RETVAL=$?\n"
"        return $RETVAL\n"
"}\n"
"\n"
"status() {\n"
"        $LOGPORT_EXECUTABLE status\n"
"        RETVAL=$?\n"
"        return $RETVAL\n"
"}\n"
"\n"
"restart() {\n"
"        $LOGPORT_EXECUTABLE restart\n"
"        RETVAL=$?\n"
"        return $RETVAL\n"
"}\n"
"\n"
"reload() {\n"
"        $LOGPORT_EXECUTABLE reload\n"
"        RETVAL=$?\n"
"        return $RETVAL\n"
"}\n"
"\n"
"case \"$1\" in\n"
"    start)\n"
"        start\n"
"        ;;\n"
"    stop)\n"
"        stop\n"
"        ;;\n"
"    status)\n"
"        status\n"
"        ;;\n"
"    restart)\n"
"        restart\n"
"        ;;\n"
"    reload)\n"
"        reload\n"
"        ;;\n"
"    *)\n"
"        echo \"Usage: $prog {start|stop|status|restart|reload}\"\n"
"        exit 1\n"
"        ;;\n"
"esac\n"
"exit $RETVAL";

		std::ofstream init_d_file;
		init_d_file.open( "/etc/init.d/logport" );
		init_d_file << init_d_file_contents;
		init_d_file.close();

		this->executeCommand( "chmod ugo+x /etc/init.d/logport" );

	}



	Database& LogPort::getDatabase(){

		if( this->db == NULL ){
			this->db = new Database();
		}

		return *this->db;

	}


}
