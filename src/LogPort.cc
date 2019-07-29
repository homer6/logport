#include "LogPort.h"
#include "Watch.h"

#include <iostream>
#include <iomanip>
using std::cout;
using std::cerr;
using std::endl;

#include <unistd.h>
#include <signal.h>

#include "InotifyWatcher.h"
#include "LevelTriggeredEpollWatcher.h"
#include "KafkaProducer.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "sqlite3.h"


#include <cstdio>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

#include "Common.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "Inspector.h"


#include <map>
using std::map;

extern char **environ;


static logport::LogPort* logport_app_ptr;

static void signal_handler_stop( int sig ){
    
    logport_app_ptr->run = false;
    logport_app_ptr->watches_paused = false;  //we "unpause" the watches so that the SIGINT can win over the pauses

    logport::Observer observer;

    switch( sig ){
    	case SIGINT: observer.addLogEntry( "logport: SIGINT received. Shutting down." ); break;
    	case SIGTERM: observer.addLogEntry( "logport: SIGTERM received. Shutting down." ); break;
    	default: observer.addLogEntry( "logport: Unknown signal received. Shutting down." );
    };
	
}

static void signal_handler_reload_config( int /*sig*/ ){

    logport_app_ptr->reload_required = true;
    logport::Observer observer;
	observer.addLogEntry( "logport: SIGHUP received. Reloading configuration." );

}

static void signal_handler_pause_resume( int sig ){

    logport::Observer observer;

    switch( sig ){

    	case SIGUSR1:
    		observer.addLogEntry( "logport: SIGUSR1 received. Stopping all watches." );
    		logport_app_ptr->watches_paused = true;
    		logport_app_ptr->run = false;    		
    		break;

    	case SIGUSR2:
    		logport_app_ptr->run = true;
    		logport_app_ptr->watches_paused = false;
    		observer.addLogEntry( "logport: SIGUSR2 received. Resuming all watches." ); 
    		logport_app_ptr->closeObserver();
    		break;

    	default:
    		observer.addLogEntry( "logport: " + logport::to_string<int>(sig)  + " received." );

    };

}


namespace logport{

	LogPort::LogPort()
		:db(NULL), inspector(NULL), observer(NULL), run(true), reload_required(false), watches_paused(false), current_version("0.2.0"), pid_filename("/var/run/logport.pid"), verbose_mode(false)
	{


	}

	LogPort::~LogPort(){

		if( this->db != NULL ){
			delete this->db;
		}

		if( this->inspector != NULL ){
			delete this->inspector;
		}

		if( this->observer != NULL ){
			delete this->observer;
		}

	}



	void LogPort::closeDatabase(){

		if( this->db != NULL ){
			delete this->db;
			this->db = NULL;
		}

	}


	void LogPort::closeObserver(){

		if( this->observer != NULL ){
			delete this->observer;
			this->observer = NULL;
		}

	}



	void LogPort::registerSignalHandlers(){

		logport_app_ptr = this;

        // Signal handler for clean shutdown 
        signal( SIGINT, signal_handler_stop );
        signal( SIGTERM, signal_handler_stop );

        signal( SIGHUP, signal_handler_reload_config );

        signal( SIGUSR1, signal_handler_pause_resume );
        signal( SIGUSR2, signal_handler_pause_resume );

	}


	void LogPort::install(){


		this->current_platform.determinePlatform();

		execute_command( "mkdir -p /usr/local/lib/logport" );
		execute_command( "cp logport /usr/local/bin" );

		//create the db
			execute_command( "mkdir -p /usr/local/logport" );
			execute_command( "chmod 777 /usr/local/logport" );

			bool database_exists = file_exists("/usr/local/logport/logport.db");
			
			{
				Database db; //creates the db
				if( !database_exists ){
					db.createDatabase();
				}

				Observer observer;  //create all of the log files

			}//explicitly closes the db so we can chmod it

			execute_command( "chmod ugo+w /usr/local/logport/logport.db" );
		
		this->installInitScript();

		this->installLogrotate();
		execute_command( "chmod o-w /usr/local/logport" );

		execute_command( "cp librdkafka.so.1 /usr/local/lib/logport" );

		cout << "Logport installed as a system service." << endl;
		cout << "Please type 'logport start' and 'logport enable' to start and enable the service to start on boot." << endl;
		cout << "The logport binary is now in your path." << endl;

	}


	void LogPort::printUnsupportedPlatform(){

		cout << "Unsupported platform. Please open an issue to request support " << endl;
		cout << "for your platform: https://github.com/homer6/logport/issues" << endl;

	}




	void LogPort::uninstall(){

		this->current_platform.determinePlatform();

		this->stop();
		this->disable();
		execute_command( "rm /etc/init.d/logport" );
		execute_command( "rm /usr/local/logport/logport.db" );
		execute_command( "rm -rf /usr/local/logport" );

		execute_command( "rm -f /etc/logrotate.d/logport" );

		cout << "Run these commands to finalize the uninstall:" << endl;
		cout << "rm /usr/local/lib/logport/librdkafka.so.1" << endl;
		cout << "rmdir /usr/local/lib/logport" << endl;
		cout << "rm /usr/local/bin/logport" << endl;

	}


	void LogPort::destroy(){

		bool is_running = this->isRunning();

		if( is_running ){
			this->stop();
		}	
		
		execute_command( "rm -rf /usr/local/logport" );
		execute_command( "mkdir -p /usr/local/logport" );
		execute_command( "chmod 777 /usr/local/logport" );

		{
			Observer observer; //create all of the log files
		}
		
		this->restoreToFactoryDefault();

		if( is_running ){
			this->start();
		}

	}


	void LogPort::restoreToFactoryDefault(){

		{
			Database db; //creates the db
			db.createDatabase();
		}//explicitly closes the db so we can chmod it

		execute_command( "chmod ugo+w /usr/local/logport/logport.db" );

	}


	void LogPort::start(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			pid_t logport_pid;
			input_pid_file >> logport_pid;
			cout << "logport service is already running - PID: " << logport_pid << endl;
			return;
		}

		cout << "Starting logport... started." << endl;

		int daemon_result = daemon( 0, 1 );
		if( daemon_result == -1 ){
			throw std::runtime_error( "Failed to fork with daemon." );
		}

		std::ofstream output_pid_file( this->pid_filename.c_str(), std::ofstream::out );
		output_pid_file << getpid();
		output_pid_file.close();
		
		this->startWatches();
		
	}


	
	void LogPort::stop(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			pid_t logport_pid;
			input_pid_file >> logport_pid;

			cout << "Stopping logport - PID: " << logport_pid << "... " << std::flush;
			kill( logport_pid, SIGTERM );
			cout << "stopped." << endl;

			remove( this->pid_filename.c_str() );

		}else{

			cout << "Service was not running." << endl;

		}

	}


	void LogPort::restart(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			pid_t pid;
			input_pid_file >> pid;

			cout << "Stopping logport - PID: " << pid << "... " << std::flush;
			kill( pid, SIGTERM );
			cout << "stopped." << endl;

		}else{

			cout << "The logport service was not running." << endl;

		}

		cout << "Restarting..." << std::flush;
		sleep(6);

		cout << " restarted." << endl;

		int daemon_result = daemon( 0, 1 );
		if( daemon_result == -1 ){
			throw std::runtime_error( "Failed to fork with daemon." );
		}

		std::ofstream output_pid_file( this->pid_filename.c_str(), std::ofstream::out );
		output_pid_file << getpid();
		output_pid_file.close();
		
		this->startWatches();

	}




	void LogPort::reload(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			pid_t logport_pid;
			input_pid_file >> logport_pid;
			kill( logport_pid, SIGHUP );
			cout << "The logport service is running (PID: " << logport_pid << ") and was signaled to reload its configuration." << endl;
		}else{
			cout << "The logport service is not running." << endl;
		}

	}


	void LogPort::reloadIfRunning(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			//service is running
			pid_t logport_pid;
			input_pid_file >> logport_pid;
			kill( logport_pid, SIGHUP );
		}

	}



	void LogPort::status(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			pid_t logport_pid;
			input_pid_file >> logport_pid;
			cout << "The logport service is running (PID: " << logport_pid << ")." << endl;
		}else{
			cout << "The logport service is not running." << endl;
		}

	}


	bool LogPort::isRunning(){

		std::ifstream input_pid_file( this->pid_filename.c_str(), std::ifstream::binary );

		if( input_pid_file ){
			return true;
		}else{
			return false;
		}

	}



	void LogPort::enable(){

		this->current_platform.determinePlatform();

		if( this->current_platform.service_manager == "systemctl" ){
			cout << execute_command( "systemctl enable logport.service" );
			return;
		}

		if( this->current_platform.service_manager == "chkconfig" ){
			cout << execute_command( "chkconfig logport on" );
			return;
		}

		this->printUnsupportedPlatform();

	}


	void LogPort::disable(){

		this->current_platform.determinePlatform();

		if( this->current_platform.service_manager == "systemctl" ){
			cout << execute_command( "systemctl disable logport.service" );
			return;
		}

		if( this->current_platform.service_manager == "chkconfig" ){
			cout << execute_command( "chkconfig logport off" );
			return;
		}

		this->printUnsupportedPlatform();

	}






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
"   now        Watches a file temporarily (same options as watch)\n"
"   adopt      Wraps around a process, sending stdout and stderr to kafka.\n"
"   destory    Restore logport to factory settings. Clears all watches.\n"
"\n"
"manage settings\n"
"   set        Set a setting's value\n"
"   unset      Clear a setting's value\n"
"   settings   List all settings\n"
"   destory    Restore logport to factory settings. Clears all watches.\n"
"\n"
"collect telemetry\n"
"   inspect    Produce telemetry to telemetry log file\n"
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
				"  -b, --brokers [BROKERS]             a csv list of kafka brokers\n"
				"                                      (optional; defaults to setting: default.brokers)\n"
				"  -t, --topic [TOPIC]                 a destination kafka topic\n"
				"                                      (optional; defaults to setting: default.topic)\n"
				"  -p, --product-code [PRODUCT_CODE]   a code identifying a part of your organization or product\n"
				"                                      (optional; defaults to setting: default.product_code)\n"
				"  -h, --hostname [HOSTNAME]           the name of this host that will appear in the log entries\n"
				"                                      (optional; defaults to setting: default.hostname)"
		<< endl;

	}

	void LogPort::printHelpSet(){

		cerr << "Usage: logport set [KEY] [VALUE]\n"
				"Sets a setting to a value.\n"
		<< endl;

	}

	void LogPort::printHelpUnset(){

		cerr << "Usage: logport unset [KEY]\n"
				"Removes a setting.\n"
		<< endl;

	}

	void LogPort::printHelpInspect(){

		cerr << "Usage: logport inspect [SUBSET]\n"
				"Produces telemetry readings to /usr/local/logport/telemetry.log\n"
				"Valid SUBSETs are 'all', 'second', '10_second', or 'day'."
		<< endl;

	}


	void LogPort::printHelpAdopt(){

		cerr << "Usage: logport adopt [OPTION]... [EXECUTABLE] [EXECUTABLE_ARGS]...\n"
				"Run an executable and capture its stdout, stderr, and exit code.\n"
				"\n"
				"Mandatory arguments to long options are mandatory for short options too.\n"
				"  -b, --brokers [BROKERS]             a csv list of kafka brokers\n"
				"                                      (optional; defaults to setting: default.brokers)\n"
				"  -t, --topic [TOPIC]                 a destination kafka topic\n"
				"                                      (optional; defaults to setting: default.topic)\n"
				"  -p, --product-code [PRODUCT_CODE]   a code identifying a part of your organization or product\n"
				"                                      (optional; defaults to setting: default.product_code)\n"
				"  -h, --hostname [HOSTNAME]           the name of this host that will appear in the log entries\n"
				"                                      (optional; defaults to setting: default.hostname)"
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

		this->loadEnvironmentVariables();


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

    	if( this->command == "watch" || this->command == "now" || this->command == "adopt" ){

    		if( argc <= 2 ){
    			if( this->command == "adopt" ){
					this->printHelpAdopt();
    			}else{    				
    				this->printHelpWatch();
    			}    			
    			return -1;
    		}

    		int current_argument_offset = 2;

    		string this_brokers = this->getDefaultBrokers();
    		string this_topic = this->getDefaultTopic();
    		string this_product_code = this->getDefaultProductCode();
    		string this_hostname = this->getDefaultHostname();

    		int number_of_added_watches = 0;

    		bool is_last_argument = false;



    		while( current_argument_offset < argc ){

    			if( current_argument_offset == argc - 1 ){
    				is_last_argument = true;
    			}


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


    			if( current_argument == "--product-code" || current_argument == "--prd" || current_argument == "-p" ){

    				current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				this_product_code = this->command_line_arguments[ current_argument_offset ];

					current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				continue;

    			}


    			if( current_argument == "--hostname" || current_argument == "-h" ){

    				current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				this_hostname = this->command_line_arguments[ current_argument_offset ];

					current_argument_offset++;
    				if( current_argument_offset >= argc ){
						this->printHelpWatch();
						return -1;
    				}
    				continue;

    			}


    			if( this->command == "adopt" ){

    				this->additional_arguments.push_back( current_argument );

    				if( is_last_argument ){

						Watch watch;
						watch.brokers = this_brokers;
						watch.topic = this_topic;
						watch.product_code = this_product_code;
						watch.hostname = this_hostname;

						this->adopt( watch );

						return 0;

    				}

    			}else{

					Watch watch;
					watch.brokers = this_brokers;
					watch.topic = this_topic;
					watch.product_code = this_product_code;
					watch.hostname = this_hostname;
					watch.watched_filepath = get_real_filepath(current_argument);
					watch.undelivered_log_filepath = watch.watched_filepath + "_undelivered";

		    		if( this->command == "now" ){
		    			this->watchNow( watch );	    			
		    		}else{
		    			this->addWatch( watch );
		    			number_of_added_watches++;
		    		}

    			}





    			current_argument_offset++;

    		}

    		if( this_product_code == "prd000" ){
    			cout << "Warning: default product_code used. Consider establishing a default (eg. 'logport set default.product_code prd123') before creating watches." << endl;
    			cout << "Please see the watch help: 'logport watch'" << endl;
    		}


    		// launch the watches if the service is running
			this->reloadIfRunning();

			cout << "Added " << number_of_added_watches << " watches." << endl;

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

    	if( this->command == "destroy" ){
    		this->destroy();
    		return 0;
    	}

    	if( this->command == "uninstall" ){
    		this->uninstall();
    		return 0;
    	}

    	if( this->command == "enable" ){
    		this->enable();
    		return 0;
    	}

    	if( this->command == "disable" ){
    		this->disable();
    		return 0;
    	}

    	if( this->command == "set" ){

    		if( argc <= 3 ){
    			this->printHelpSet();
    			return -1;
    		}

    		string key = this->command_line_arguments[ 2 ];
    		string value = this->command_line_arguments[ 3 ];

    		this->addSetting( key, value );

    		return 0;
    	}



    	if( this->command == "unset" ){

    		if( argc <= 2 ){
    			this->printHelpUnset();
    			return -1;
    		}

    		string key = this->command_line_arguments[ 2 ];

    		this->removeSetting( key );

    		return 0;
    	}


    	if( this->command == "settings" ){
    		this->listSettings();
    		return 0;
    	}

    	if( this->command == "start" ){
    		this->start();
    		return 0;
    	}

    	if( this->command == "stop" ){
    		this->stop();
    		return 0;
    	}

    	if( this->command == "restart" ){
    		this->restart();
    		return 0;
    	}

    	if( this->command == "reload" ){
    		this->reload();
    		return 0;
    	}

    	if( this->command == "status" ){
    		this->status();
    		return 0;
    	}
    	
    	if( this->command == "inspect" ){

    		if( argc <= 2 ){
    			this->printHelpInspect();
    			return -1;
    		}

    		string subset = this->command_line_arguments[ 2 ];

    		Inspector& inspector = this->getInspector();

    		if( subset == "all" ){
    			inspector.monitorTwoSecondsTick();
    			inspector.monitorTenSecondsTick();
    			inspector.monitorDayTick();
    		}else if( subset == "second" ){
    			inspector.monitorTwoSecondsTick();
    		}else if( subset == "10_second" ){
    			inspector.monitorTenSecondsTick();
    		}else if( subset == "day" ){
    			inspector.monitorDayTick();
    		}

    		return 0;

    	}

    	this->printHelp();

    	return 1;

    }


    string LogPort::getEnvironmentVariable( const string& variable_name ) const{

    	if( this->environment_variables.count(variable_name) != 0 ){
    		return this->environment_variables.at(variable_name);
    	}

    	return "";

    }

    void LogPort::setEnvironmentVariable( const string& variable_name, const string& variable_value ){

    	this->environment_variables.insert( std::pair<string,string>(variable_name,variable_value) );

    }


	string LogPort::getDefaultTopic(){

		string default_topic_env = this->getEnvironmentVariable( "LOGPORT_TOPIC" );
		if( default_topic_env.size() > 0 ){
			return default_topic_env;
		}

		string default_topic = this->getSetting( "default.topic" );

		if( default_topic.size() > 0 ){
			return default_topic;
		}

		return "logport_logs";

	}


	string LogPort::getDefaultBrokers(){

		string default_brokers_env = this->getEnvironmentVariable( "LOGPORT_BROKERS" );
		if( default_brokers_env.size() > 0 ){
			return default_brokers_env;
		}

		string default_brokers = this->getSetting( "default.brokers" );

		if( default_brokers.size() > 0 ){
			return default_brokers;
		}

		return "localhost:9092";

	}


	string LogPort::getDefaultProductCode(){

		string default_product_code_env = this->getEnvironmentVariable( "LOGPORT_PRODUCT_CODE" );
		if( default_product_code_env.size() > 0 ){
			return default_product_code_env;
		}

		string default_product_code = this->getSetting( "default.product_code" );

		if( default_product_code.size() > 0 ){
			return default_product_code;
		}

		return "prd000";

	}


	string LogPort::getDefaultHostname(){

		string default_hostname_env = this->getEnvironmentVariable( "LOGPORT_HOSTNAME" );
		if( default_hostname_env.size() > 0 ){
			return default_hostname_env;
		}

		string default_hostname = this->getSetting( "default.hostname" );

		if( default_hostname.size() > 0 ){
			return default_hostname;
		}

		return get_hostname();

	}



	void LogPort::listWatches(){

		Database& db = this->getDatabase();

		vector<Watch> watches = db.getWatches();

		if( watches.size() == 0 ){
			cout << "No files are being watched." << endl;
			return;
		}

		vector<string> column_labels;
		column_labels.push_back( " watch_id" ); //add a space for left padding
		column_labels.push_back( "watched_filepath" );
		column_labels.push_back( "brokers" );
		column_labels.push_back( "topic" );
		column_labels.push_back( "product_code" );
		column_labels.push_back( "hostname" );
		column_labels.push_back( "file_offset_sent" );
		column_labels.push_back( "pid" );

		int num_table_columns = column_labels.size();

		//calculate the widths of the columns so they fit nicely

			//the vector index is the column number (first column 0)
			vector<size_t> column_widths_maximums( num_table_columns, 0 ); //all zeros as initial values

			//account for the column data
			for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

				Watch& watch = *it;

				// id
					std::ostringstream id_stringstream;
					id_stringstream << watch.id;
					string id_string = id_stringstream.str();

					if( id_string.size() > column_widths_maximums[0] ){
						column_widths_maximums[0] = id_string.size();
					}

				// watched_filepath
					if( watch.watched_filepath.size() > column_widths_maximums[1] ){
						column_widths_maximums[1] = watch.watched_filepath.size();
					}
					
				// brokers
					if( watch.brokers.size() > column_widths_maximums[2] ){
						column_widths_maximums[2] = watch.brokers.size();
					}

				// topic
					if( watch.topic.size() > column_widths_maximums[3] ){
						column_widths_maximums[3] = watch.topic.size();
					}

				// product_code
					if( watch.product_code.size() > column_widths_maximums[4] ){
						column_widths_maximums[4] = watch.product_code.size();
					}

				// hostname
					if( watch.hostname.size() > column_widths_maximums[5] ){
						column_widths_maximums[5] = watch.hostname.size();
					}

				// file_offset
					std::ostringstream file_offset_stringstream;
					file_offset_stringstream << watch.file_offset;
					string file_offset_string = file_offset_stringstream.str();

					if( file_offset_string.size() > column_widths_maximums[6] ){
						column_widths_maximums[6] = file_offset_string.size();
					}

				// pid
					std::ostringstream pid_stringstream;
					pid_stringstream << watch.pid;
					string pid_string = pid_stringstream.str();

					if( pid_string.size() > column_widths_maximums[7] ){
						column_widths_maximums[7] = pid_string.size();
					}

			}


			// account for the column header widths
			for( int x = 0; x < num_table_columns; x++ ){
				if( column_labels[x].size() > column_widths_maximums[x] ){
					column_widths_maximums[x] = column_labels[x].size();
				}
			}



		int character_column_count = 0;
	

		// print the column headers (labels)
			for( int x = 0; x < num_table_columns; x++ ){

				int column_width = column_widths_maximums[x];

				if( x + 1 == num_table_columns ){
					//last column
					cout << std::left << std::setw(column_width) << column_labels[x] << endl; 
				}else{
					cout << std::left << std::setw(column_width) << column_labels[x] << " | "; 
				}

				character_column_count += column_width;

			}

		// print the underline for the column headers
			cout << string( character_column_count + ( num_table_columns * 3 ) - 2, '-') << endl;


		// print all of the records
		for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

			Watch& watch = *it;

			cout << std::right << std::setw(column_widths_maximums[0]) << watch.id << " | ";
			cout << std::left << std::setw(column_widths_maximums[1]) << watch.watched_filepath << " | ";
			cout << std::left << std::setw(column_widths_maximums[2]) << watch.brokers << " | ";
			cout << std::left << std::setw(column_widths_maximums[3]) << watch.topic << " | ";
			cout << std::left << std::setw(column_widths_maximums[4]) << watch.product_code << " | ";
			cout << std::left << std::setw(column_widths_maximums[5]) << watch.hostname << " | ";
			cout << std::right << std::setw(column_widths_maximums[6]) << watch.file_offset << " | ";
			cout << std::right << std::setw(column_widths_maximums[7]) << watch.pid << endl;

		}

	}



	void LogPort::addWatch( const Watch& watch ){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "INSERT INTO watches ( filepath, file_offset, brokers, topic, product_code, hostname, pid ) VALUES ( ?, ?, ?, ?, ?, ?, ? );" );

		watch.bind( statement, true );

		statement.step();
		statement.reset();
		statement.clearBindings();

	}


	bool LogPort::addSetting( const string& key, const string& value ){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "INSERT OR REPLACE INTO settings ( key, value ) VALUES ( ?, ? );" );

		statement.bindText( 0, key );
		statement.bindText( 1, value );

		try{

			statement.step();
			return true;

		}catch( ... ){

			return false;

		}

	}



	string LogPort::getSetting( const string& key ){

		Database& db = this->getDatabase();

		return db.getSetting( key );

	}


	bool LogPort::removeSetting( const string& key ){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "DELETE FROM settings WHERE key = ?;" );

		statement.bindText( 0, key );

		try{

			statement.step();
			return true;

		}catch( ... ){

			return false;

		}

	}



	void LogPort::listSettings(){

		Database& db = this->getDatabase();

		map<string,string> settings = db.getSettings();

		if( settings.size() == 0 ){
			cout << "No settings found." << endl;
			return;
		}

		vector<string> column_labels;
		column_labels.push_back( " key" ); //add a space for left padding
		column_labels.push_back( "value" );

		int num_table_columns = column_labels.size();

		//calculate the widths of the columns so they fit nicely

			//the vector index is the column number (first column 0)
			vector<size_t> column_widths_maximums( num_table_columns, 0 ); //all zeros as initial values

			//account for the column data
			for( map<string,string>::iterator it = settings.begin(); it != settings.end(); ++it ){

				string key = it->first;
				string value = it->second;

				// key
					size_t key_size = key.size() + 1; //add 1 for left padding
					if( key_size > column_widths_maximums[0] ){
						column_widths_maximums[0] = key_size;
					}
					
				// value
					if( value.size() > column_widths_maximums[1] ){
						column_widths_maximums[1] = value.size();
					}

			}


			// account for the column header widths
			for( int x = 0; x < num_table_columns; x++ ){
				if( column_labels[x].size() > column_widths_maximums[x] ){
					column_widths_maximums[x] = column_labels[x].size();
				}
			}



		int character_column_count = 0;
	

		// print the column headers (labels)
			for( int x = 0; x < num_table_columns; x++ ){

				int column_width = column_widths_maximums[x];

				if( x + 1 == num_table_columns ){
					//last column
					cout << std::left << std::setw(column_width) << column_labels[x] << endl; 
				}else{
					cout << std::left << std::setw(column_width) << column_labels[x] << " | "; 
				}

				character_column_count += column_width;

			}

		// print the underline for the column headers
			cout << string( character_column_count + ( num_table_columns * 3 ) - 2, '-') << endl;


		// print all of the records
		for( map<string,string>::iterator it = settings.begin(); it != settings.end(); ++it ){

			string key = it->first;
			string value = it->second;

			cout << std::left << std::setw(column_widths_maximums[0]) << " " + key << " | ";
			cout << std::left << std::setw(column_widths_maximums[1]) << value << endl;

		}

	}




	void LogPort::watchNow( const Watch& /*watch*/ ) const{

		/*

		KafkaProducer kafka_producer( watch.brokers, watch.topic, watch.undelivered_log_filepath );  

		InotifyWatcher watcher( watch.watched_filepath, watch.undelivered_log_filepath, kafka_producer );  //expects undelivered log to exist
		//inotify_watcher_ptr = &watcher;

		watcher.watch(); //main loop; blocks

		*/

	}



	#define IO_BUFFER_SIZE 64 * 1024


	void LogPort::adopt( const Watch& watch ){

		Watch current_watch = watch;


		if( this->additional_arguments.size() == 0 ){
			this->printHelpAdopt();
			return;
		}

		if( this->additional_arguments.size() < 1 ){
			throw std::runtime_error( "Invalid additional_arguments." );
		}

		string executable = this->additional_arguments[0];
		string process_description = executable;

		vector<string> arguments;

		//drop the executable from the passed arguments (first argument)
			vector<string>::iterator it = this->additional_arguments.begin();
			it++;
			std::copy( it, this->additional_arguments.end(), std::back_inserter(arguments) );

		map<string,string> env_vars = this->environment_variables;

		//process any environment variables here, if necessary


		/*
		for( vector<string>::const_iterator it = this->additional_arguments.begin(); it != this->additional_arguments.end(); it++ ){
			cout << "additional_arg: " << *it << endl;
		}
		*/



		//create pipes for stdin, stdout, and stderr
			int child_stdin_pipe[2];
			int child_stdout_pipe[2];
			int child_stderr_pipe[2];

			if( pipe(child_stdin_pipe) != 0 ){
				this->getObserver().addLogEntry( "logport: failed to create stdin pipe" );
				return;
			}

			if( pipe(child_stdout_pipe) != 0 ){
				this->getObserver().addLogEntry( "logport: failed to create stdout pipe" );
				return;
			}

			if( pipe(child_stderr_pipe) != 0 ){
				this->getObserver().addLogEntry( "logport: failed to create stderr pipe" );
				return;
			}


		//start the child process
			pid_t child_process_id = this->startProcess( executable, process_description, arguments, env_vars, child_stdin_pipe, child_stdout_pipe, child_stderr_pipe );


		if( child_process_id == -1 ){
			this->getObserver().addLogEntry( "logport: failed to start adopted executable" );
			return;
		}


		//close unused ends of the pipes
			close( child_stdin_pipe[0] );
			close( child_stdout_pipe[1] );
			close( child_stderr_pipe[1] );


		//LevelTriggeredEpollWatcher stdin_watcher( STDIN_FILENO );
		LevelTriggeredEpollWatcher child_stdout_watcher( child_stdout_pipe[0] );
		LevelTriggeredEpollWatcher child_stderr_watcher( child_stderr_pipe[0] );

		Database db;
		map<string,string> settings = db.getSettings();

		KafkaProducer kafka_producer( settings, this, watch.brokers, watch.topic, watch.undelivered_log_filepath );

		bool continue_reading = true;

		int bytes_read;
		char buffer[IO_BUFFER_SIZE];

		string previous_stdout_partial;
		string previous_stderr_partial;

		int status;

		//int x = 0;

		while( continue_reading ){

			//cout << x++ << endl;

			//WNOHANG makes this return immediately (so we can continue to service the pipes)
			pid_t current_child_pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED );

			//cout << "current_child_pid == -1" << endl;

			if( current_child_pid == -1 ){
				//error, or no child processes
                this->getObserver().addLogEntry( "logport: adopt waitpid() error or no watches present. errno: " + logport::to_string<int>(errno) );
                continue_reading = false;

                //cout << "error, or no child processes" << endl;
			}

			//cout << "current_child_pid == 0" << endl;

			if( current_child_pid == 0 ){
				//no children have exited

				//cout << "no children have exited" << endl;

			}

			//cout << "current_child_pid > 0" << endl;

			if( current_child_pid > 0 ){

				string log_line;

				//cout << "current_child_pid: " << current_child_pid << endl;

				// log the exit code
				if( WIFEXITED(status) ){

					int exit_status = WEXITSTATUS(status);
					log_line = "logport: PID (" + logport::to_string<pid_t>(current_child_pid) + ") exited with status " + logport::to_string<int>(exit_status);
					continue_reading = false;

				}else if( WIFSIGNALED(status) ){

					int signal_number = WTERMSIG(status);
					log_line = "logport: PID (" + logport::to_string<pid_t>(current_child_pid) + ") killed by signal " + logport::to_string<int>(signal_number);
					continue_reading = false;

				}else if( WIFSTOPPED(status) ){

					int signal_number = WSTOPSIG(status);
					log_line = "logport: PID (" + logport::to_string<pid_t>(current_child_pid) + ") stopped by signal " + logport::to_string<int>(signal_number);

				}else if( WIFCONTINUED(status) ){

					log_line = "logport: PID (" + logport::to_string<pid_t>(current_child_pid) + ") continued";

				}else{

					log_line = "logport: PID (" + logport::to_string<pid_t>(current_child_pid) + ") unknown return from waitpid "  + logport::to_string<int>(status);

				}

				current_watch.watched_filepath = "process_exit";
				string filtered_log_line = current_watch.filterLogLine( log_line );
                kafka_producer.produce( filtered_log_line );
				
			}


			//cout << "stdin_watcher.watch(0)" << endl;

			/*
			if( stdin_watcher.watch(0) ){  //returns immediately if there are inotify events waiting; returns after 0ms if no events;

				//cout << "reading from stdin" << endl;

                bytes_read = read( STDIN_FILENO, buffer, IO_BUFFER_SIZE );

                //cout << "stdin bytes read: " << bytes_read << endl;

                if( bytes_read > 0 ){

                	//pass the stdin contents to the child process
                	int write_result = write( child_stdin_pipe[1], buffer, bytes_read );

                	if( write_result == -1 ){
					
	                	int e = errno;

						if( e ){
							string error_string( strerror(e) );
							observer.addLogEntry( "logport: failed to write to child_stdin_pipe: " + error_string );
		               	}					

					}
  
                }else{

                    //no bytes read (EOF)

                }

			}else{

                //no events waiting on stdin; timed out watching for 0ms

            }
            */


            //cout << "child_stdout_watcher.watch(1000)" << endl;

			if( child_stdout_watcher.watch(1000) ){  //returns immediately if there are inotify events waiting; returns after 1000ms if no events;

				//cout << "reading from stdout" << endl;
				
                bytes_read = read( child_stdout_pipe[0], buffer, IO_BUFFER_SIZE );

                //cout << "child_stdout_pipe bytes read: " << bytes_read << endl;

                if( bytes_read > 0 ){

                    string stdout_chunk( buffer, bytes_read );

                    //append previous, if applicable
                        if( previous_stdout_partial.size() ){
                            stdout_chunk = previous_stdout_partial + stdout_chunk;
                            previous_stdout_partial.clear();
                        }

                    //send multiple lines as multiple kafka messages (no need to batch because rdkafka batches internally)
                    //no partial line will ever be sent
                        string::iterator current_message_begin_it = stdout_chunk.begin();
                        string::iterator current_message_end_it = stdout_chunk.begin();

                        char current_char;

                        while( current_message_end_it != stdout_chunk.end() ){
                                                                
                            current_char = *current_message_end_it; //safe to deref because "bytes_read > 0" above

                            if( current_char == '\n' ){

                                string sent_message;
                                //sent_message.reserve( stdout_chunk.size() );

                                //only copy if there's something to copy
                                if( current_message_end_it != current_message_begin_it ){
                                    std::copy( current_message_begin_it, current_message_end_it, std::back_inserter(sent_message) );  //drops the new line    
                                }

                                

                                if( sent_message.size() > 0 ){

                                	current_watch.watched_filepath = "stdout";
                                    string filtered_log_line = current_watch.filterLogLine( sent_message );

                                    //handle consecutive newline characters (by dropping them)
					                kafka_producer.produce( filtered_log_line );
                                    kafka_producer.poll();

                                    //also write to this stdout
                                    std::cout << sent_message << std::endl;
                                    
                                    //skips the new line
                                    current_message_end_it++;

                                    //re-sync the being and end iterators
                                    current_message_begin_it = current_message_end_it;

                                    /*
                                    std::cout << "stdout_chunk(" << stdout_chunk.size() << "): " << std::endl;
                                    std::cout << "unfiltered_message(" << sent_message.size() << "): " << sent_message << std::endl;
                                    std::cout << "filtered_sent_message(" << filtered_log_line.size() << "): " << filtered_log_line << std::endl;
                                    std::cout << "previous_stdout_partial(" << previous_stdout_partial.size() << "): " << previous_stdout_partial << std::endl;
									*/

                                }else{

                                	current_message_end_it++;

                                }

                            }else{

                                current_message_end_it++;

                            }

                        }

                        //if we're at the end and we have an incomplete message, copy remaining incomplete line
                        if( current_message_begin_it != current_message_end_it ){
                            //previous_stdout_partial.reserve( stdout_chunk.size() );
                            std::copy( current_message_begin_it, stdout_chunk.end(), std::back_inserter(previous_stdout_partial) );
                        }



                }else{

                    //no bytes read (EOF)

                }

			}else{

                //no events waiting on child's stdout; timed out watching for 1000ms

            }


            //cout << "child_stderr_watcher.watch(0)" << endl;


			if( child_stderr_watcher.watch(0) ){  //returns immediately if there are inotify events waiting; returns after 0ms if no events;

				//cout << "reading from stderr" << endl;

				bytes_read = read( child_stderr_pipe[0], buffer, IO_BUFFER_SIZE );

				//cout << "child_stderr_pipe read: " << bytes_read << endl;

                if( bytes_read > 0 ){

                    string stderr_chunk( buffer, bytes_read );

                    //append previous, if applicable
                        if( previous_stderr_partial.size() ){
                            stderr_chunk = previous_stderr_partial + stderr_chunk;
                            previous_stderr_partial.clear();
                        }

                    //send multiple lines as multiple kafka messages (no need to batch because rdkafka batches internally)
                    //no partial line will ever be sent
                        string::iterator current_message_begin_it = stderr_chunk.begin();
                        string::iterator current_message_end_it = stderr_chunk.begin();

                        char current_char;

                        while( current_message_end_it != stderr_chunk.end() ){
                                                                
                            current_char = *current_message_end_it; //safe to deref because "bytes_read > 0" above

                            if( current_char == '\n' ){

                                string sent_message;
                                //sent_message.reserve( stderr_chunk.size() );

                                //only copy if there's something to copy
                                if( current_message_end_it != current_message_begin_it ){
                                    std::copy( current_message_begin_it, current_message_end_it, std::back_inserter(sent_message) );  //drops the new line    
                                }

                                

                                if( sent_message.size() > 0 ){

                                	current_watch.watched_filepath = "stderr";
                                    string filtered_log_line = current_watch.filterLogLine( sent_message );

                                    //handle consecutive newline characters (by dropping them)
					                kafka_producer.produce( filtered_log_line );
                                    kafka_producer.poll();

                                    //also write to this stderr
                                    std::cerr << sent_message << std::endl;
                                    
                                    //skips the new line
                                    current_message_end_it++;

                                    //re-sync the being and end iterators
                                    current_message_begin_it = current_message_end_it;

                                    /*
                                    std::cout << "stderr_chunk(" << stderr_chunk.size() << "): " << std::endl;
                                    std::cout << "unfiltered_message(" << sent_message.size() << "): " << sent_message << std::endl;
                                    std::cout << "filtered_sent_message(" << filtered_log_line.size() << "): " << filtered_log_line << std::endl;
                                    std::cout << "previous_stderr_partial(" << previous_stderr_partial.size() << "): " << previous_stderr_partial << std::endl;
                                    */
                                }else{

                                	current_message_end_it++;
                                	
                                }


                            }else{

                                current_message_end_it++;

                            }

                        }

                        //if we're at the end and we have an incomplete message, copy remaining incomplete line
                        if( current_message_begin_it != current_message_end_it ){
                            //previous_stderr_partial.reserve( stderr_chunk.size() );
                            std::copy( current_message_begin_it, stderr_chunk.end(), std::back_inserter(previous_stderr_partial) );
                        }


                    }else{

                        //no bytes read (EOF)

                    }

			}else{

                //no events waiting on child's stderr; timed out watching for 334ms

            }

            /* A producer application should continually serve
             * the delivery report queue by calling rd_kafka_poll()
             * at frequent intervals.
             * Either put the poll call in your main loop, or in a
             * dedicated thread, or call it after every
             * rd_kafka_produce() call.
             * Just make sure that rd_kafka_poll() is still called
             * during periods where you are not producing any messages
             * to make sure previously produced messages have their
             * delivery report callback served (and any other callbacks
             * you register). */
            kafka_producer.poll();


		} //continue reading loop

		kafka_producer.poll( 1000 );

	}



	void LogPort::loadEnvironmentVariables(){

		int i = 0;
		while( environ[i] ){

			string environment_line( environ[i] ); // in the form of "variable=value"
			i++;

			std::string::size_type n = environment_line.find('=');

			if( n == std::string::npos ){
				//not found
				throw std::runtime_error("Unexpected environment format.");
			} else {				
				string variable_name = environment_line.substr(0, n);
				string variable_value = environment_line.substr(n + 1);
				this->environment_variables.insert( std::pair<string,string>(variable_name, variable_value) );
			}

		}

	}




	pid_t LogPort::startProcess( const string& executable_path, const string& process_description, const std::vector<string>& arguments, const std::map<string,string>& environment_variables, int *child_stdin_pipe, int *child_stdout_pipe, int *child_stderr_pipe ){

		pid_t pid = fork();

		if( pid == 0 ){
			//child

			//redirect stdin to the read end of the stdin pipe
			dup2( child_stdin_pipe[0], STDIN_FILENO );

			//redirect stdout to the write end of the stdout pipe
			dup2( child_stdout_pipe[1], STDOUT_FILENO );

			//redirect stderr to the write end of the stderr pipe
			dup2( child_stderr_pipe[1], STDERR_FILENO );


			//close unused ends of the pipes
				close( child_stdin_pipe[1] );
				close( child_stdout_pipe[0] );
				close( child_stderr_pipe[0] );


			//const char* child_args[] = { process_description.c_str(), NULL };
			//const char* child_envp[] = { "HELP=doggy", NULL };

			//populate the child_args array
				size_t num_arguments = arguments.size();

				if( num_arguments > 1023 ){
					throw std::runtime_error("Too many arguments (" + logport::to_string<size_t>(num_arguments) + "/1023).");
				}


				string full_executable_path = get_executable_filepath( executable_path );

				//this must be deleted later; throws on new fail
				char** child_args = new char*[ num_arguments + 2 ];

				int x = 0;
				child_args[x] = const_cast<char*>( process_description.c_str() );
				++x;
				for( vector<string>::const_iterator it = arguments.begin(); it != arguments.end(); it++ ){
					child_args[x] = const_cast<char*>( (*it).c_str() );
					++x;
				}
				child_args[x] = NULL;



			//merge the two fields back into the canonical environment variable string
			//this memory must persist until child_envp is done with it

				if( environment_variables.size() > 255 ){
					delete child_args;
					throw std::runtime_error("Too many environment variables (" + logport::to_string<size_t>(environment_variables.size()) + "/255).");
				}

				std::vector<string> child_env_vec;
				for( std::map<string,string>::const_iterator it = environment_variables.begin(); it != environment_variables.end(); it++ ){
					child_env_vec.push_back( it->first + "=" + it->second );
				}

				//this must be deleted later; throws on new fail
				char** child_envp = new char*[ environment_variables.size() + 1 ];

				x = 0;
				for( vector<string>::iterator it = child_env_vec.begin(); it != child_env_vec.end(); it++ ){
					child_envp[x] = const_cast<char*>( (*it).c_str() );
					++x;
				}
				child_envp[x] = NULL;


			//start the process
			int result = execve( full_executable_path.c_str(), child_args, child_envp );

			if( errno == ENOEXEC ){
				throw std::runtime_error("Executable not found. Please ensure that you either provide the full, absolute path or the executable is in your PATH." );
			}


			//result will always be -1 here because execve doesn't return if successful
			if( result == -1 ){

				int e = errno;

				if( e ){
					string error_string( strerror(e) );
					Observer observer;
					observer.addLogEntry( "Failed to execve: " + error_string );
				}

			}
			delete[] child_args;
			delete[] child_envp;

			exit(1);

		}else if( pid == -1 ){

			//error

			int e = errno;

			if( e ){
				string error_string( strerror(e) );
				Observer observer;
				observer.addLogEntry( "Failed to fork: " + error_string );
			}


		}else{
			//parent
			
			return pid;

		}

		return -1;

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

		execute_command( "chmod ugo+x /etc/init.d/logport" );

	}



	void LogPort::installLogrotate(){

		const char *logrotate_file_contents = 
"/usr/local/logport/*.log\n"
"{\n"
"        rotate 7\n"
"        daily\n"
"        delaycompress\n"
"        missingok\n"
"        notifempty\n"
"        compress\n"
"        firstaction\n"
"            kill -USR1 `cat /var/run/logport.pid`\n"
"        endscript\n"
"        lastaction\n"
"            kill -USR2 `cat /var/run/logport.pid`\n"
"        endscript\n"
"}\n";

		std::ofstream init_d_file;
		init_d_file.open( "/etc/logrotate.d/logport" );
		init_d_file << logrotate_file_contents;
		init_d_file.close();

	}



	Database& LogPort::getDatabase(){

		if( this->db == NULL ){
			this->db = new Database();
		}

		return *this->db;

	}

	Inspector& LogPort::getInspector(){

		if( this->inspector == NULL ){
			this->inspector = new Inspector();
		}

		return *this->inspector;

	}


	Observer& LogPort::getObserver(){

		if( this->observer == NULL ){
			this->observer = new Observer();
		}

		return *this->observer;

	}


	void LogPort::waitUnlessEvent( int seconds ){

		if( seconds <= 0 ) return;

		int current_second = 0;

		while( current_second < seconds ){

			sleep( 1 );

			if( this->run == false || this->reload_required == true ){
				return;
			}

			current_second++;
		}

	}


	void LogPort::startWatches(){

		this->getObserver().addLogEntry( "logport: started" );


		vector<Watch> watches;

		{
			Database db;
			watches = db.getWatches();
		}

		 
		if( watches.size() == 0 ){

			this->getObserver().addLogEntry( "Started logport service with no files being watched." );

		}else{

			for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){
				Watch& watch = *it;
				//this forks for each watch
				watch.start( this );
			}

		}


		bool have_initiated_all_stop = false;
		bool shutdown_complete = false;

		bool initiate_resuming_of_watches = false;


		int status;

		while( this->run || ( !shutdown_complete && !have_initiated_all_stop ) || this->watches_paused ){

			//WNOHANG makes this return immediately (so we can monitor RSS)
			pid_t child_pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED );
			//pid_t child_pid = waitpid( -1, &status, WUNTRACED | WCONTINUED );


			/*
			cout << "-------------------" << endl;
			cout << "this->run: " << this->run << endl;
			cout << "shutdown_complete: " << shutdown_complete << endl;
			cout << "this->watches_paused " << this->watches_paused << endl;
			cout << "have_initiated_all_stop " << have_initiated_all_stop << endl;
			cout << "initiate_resuming_of_watches " << initiate_resuming_of_watches << endl;
			*/



			// determine if we should resume watches after they were paused
				if( shutdown_complete && this->run == true && this->watches_paused == false && have_initiated_all_stop == true ){

					initiate_resuming_of_watches = true;

					//reset others to resume normal operations
					have_initiated_all_stop = false;
					shutdown_complete = false;

				}


			//determine if all watches have shutdown
				if( this->run == false && have_initiated_all_stop == true && shutdown_complete == false ){

					bool some_watches_still_running = false;

					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){
						Watch& watch = *it;
						if( watch.pid > 0 ){
							some_watches_still_running = true;
						}
					}

					if( !some_watches_still_running ){
						shutdown_complete = true;
					}

				}


			//determine if some watches should startup (if reload is required or resuming watches)
				if( ( this->reload_required == true || initiate_resuming_of_watches == true ) && this->run == true && have_initiated_all_stop == false ){

					//reload from db
					{
						Database db;
						watches = db.getWatches();
					}

					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){
						Watch& watch = *it;
						if( watch.pid < 0 ){
							//not running watch; let's start it
							watch.start( this );
						}
					}

					if( this->reload_required ){
						this->reload_required = false;
					}					

					if( initiate_resuming_of_watches ){
						initiate_resuming_of_watches = false;
					}

					//reset others to resume normal operations
					shutdown_complete = false;
					have_initiated_all_stop = false;

				}



			if( child_pid == -1 ){
				//error, or no child processes
                this->getObserver().addLogEntry( "logport: waitpid() error or no watches present. errno: " + logport::to_string<int>(errno) );
                if( have_initiated_all_stop ){
                	shutdown_complete = true;
                }               	
				this->waitUnlessEvent( 5 );
			}

			if( child_pid == 0 ){
				//no children have exited


				//stop all children
					if( this->run == false && have_initiated_all_stop == false ){

						Database db;

						this->getObserver().addLogEntry( "logport: gracefully stopping all watches..." );

						for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

							Watch& watch = *it;

							if( watch.pid > 0 ){
								watch.last_pid = watch.pid;
								
								//we're not going to use watch.stop() because it contains a shutdown delay and we want the delays to run in parallel (without needing to add threading, if possible)
								//watch.stop( this );

						        //terminate gracefully first, then forcefully if graceful shutdown lasts longer than 20s
						        if( kill(watch.pid, SIGINT) == -1 ){
						            this->getObserver().addLogEntry( "logport: failed to kill watch with SIGINT." );
						        }
						        
								watch.pid = -1;
								watch.savePid( db );

							}

						}

						have_initiated_all_stop = true;

					}


				//ensure each watch is not using more than 250MB of memory

				if( this->run == true && have_initiated_all_stop == false ){

					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

						Watch& watch = *it;

						if( watch.pid > 0 ){
							//if there's an existing process (ie. not the first loop iteration)

							int watch_process_rss = proc_status_get_rss_usage_in_kb( watch.pid );
							const string process_name = proc_status_get_name( watch.pid );

							vector<string> proc_stats = proc_stat_values( watch.pid );
							double total_time_seconds = 0;
							unsigned long user_time_ticks = 0;
							unsigned long kernel_time_ticks = 0;
							if( proc_stats.size() > 23 ){
								user_time_ticks = string_to_ulong( proc_stats[14] );
								kernel_time_ticks = string_to_ulong( proc_stats[15] );
								long clock_ticks_per_second = sysconf(_SC_CLK_TCK);
								if( clock_ticks_per_second == 0 ){
									clock_ticks_per_second = 100;
								}
								total_time_seconds = double(user_time_ticks + kernel_time_ticks) / double(clock_ticks_per_second);
							}

							this->getObserver().addLogEntry( "logport: watch process_name(" + process_name + "), RSS(" + logport::to_string<int>(watch_process_rss) + "KB), PID(" + logport::to_string<pid_t>(watch.pid) + "), cpu_time(" + logport::to_string<double>(total_time_seconds) + ")" );

							if( watch_process_rss > 250000 ){ //250MB
								//kill -9
								//wait
								//respawn

								this->getObserver().addLogEntry( "logport: watch (pid: " + logport::to_string<pid_t>(watch.pid) + ", file: " + watch.watched_filepath + ") was killed because the RSS exceeded 250MB." );

								watch.last_pid = watch.pid;

								watch.stop( this );

								sleep(5);
								
								watch.start( this );
								if( watch.last_pid == watch.pid ){

									this->getObserver().addLogEntry( "logport: watch was killed and the new process has the same PID. Exiting." );

								}

								sleep(5);

							}



							if( total_time_seconds > 1800 ){ //30 minutes
								//kill -9
								//wait
								//respawn

								this->getObserver().addLogEntry( "logport: watch (pid: " + logport::to_string<pid_t>(watch.pid) + ", file: " + watch.watched_filepath + ") was killed because the CPU time exceeded 30 minutes." );

								watch.last_pid = watch.pid;

								watch.stop( this );

								sleep(5);
								
								watch.start( this );
								if( watch.last_pid == watch.pid ){

									this->getObserver().addLogEntry( "logport: watch was killed and the new process has the same PID. Exiting." );

								}

								sleep(5);

							}


						}


					}

					this->waitUnlessEvent( 60 );
					continue;

				}


			}



			bool just_killed = false;
			for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

				Watch& watch = *it;

				if( child_pid == watch.last_pid ){
					//we just killed this process; ignore that it died
					watch.last_pid = -1;
					this->getObserver().addLogEntry( "logport: Notified of last PID." );
					just_killed = true;
				}

			}
			if( just_killed ){
				continue;
			}
			

			if( child_pid > 0 ){

				//find current watch
					Watch* current_watch = NULL;
					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

						Watch& watch = *it;

						if( child_pid == watch.pid ){
							current_watch = &watch;
						}

					}

				// restart, if necessary
				if( WIFEXITED(status) ){

					int exit_status = WEXITSTATUS(status);
					this->getObserver().addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") exited with status " + logport::to_string<int>(exit_status) );
					
					if( current_watch != NULL && this->run == true ){
						this->getObserver().addLogEntry( "restarting..." );
						current_watch->last_pid = child_pid;
						current_watch->start( this );
					}


				}else if( WIFSIGNALED(status) ){

					int signal_number = WTERMSIG(status);
					this->getObserver().addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") killed by signal " + logport::to_string<int>(signal_number) );
					
					if( current_watch != NULL && this->run == true ){
						this->getObserver().addLogEntry( "restarting..." );
						current_watch->last_pid = child_pid;
						current_watch->start( this );
					}


				}else if( WIFSTOPPED(status) ){

					int signal_number = WSTOPSIG(status);
					this->getObserver().addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") stopped by signal " + logport::to_string<int>(signal_number) );
					if( current_watch != NULL && this->run == true ){
						current_watch->last_pid = child_pid;
						current_watch->pid = -1;
						Database db;
						current_watch->savePid( db );
					}

				}else if( WIFCONTINUED(status) ){

					this->getObserver().addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") continued" );


				}else{

					this->getObserver().addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") unknown return from waitpid " + logport::to_string<int>(status) );

				}

			}

		}

		this->getObserver().addLogEntry( "logport: service shutdown complete" );

	}







}
