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

#include "Common.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <errno.h>
#include <signal.h>

#include "Inspector.h"


#include <map>
using std::map;


static logport::LogPort* logport_app_ptr;

static void signal_handler_stop( int sig ){
    
    logport_app_ptr->run = false;

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


namespace logport{

	LogPort::LogPort()
		:db(NULL), inspector(NULL), observer(NULL), run(true), reload_required(false), current_version("0.1.0"), pid_filename("/var/run/logport.pid"), verbose_mode(false)
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
        signal( SIGINT | SIGTERM, signal_handler_stop );
        signal( SIGHUP, signal_handler_reload_config );

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
		sleep(1);

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

    	if( this->command == "watch" || this->command == "now" ){

    		if( argc <= 2 ){
    			this->printHelpWatch();
    			return -1;
    		}

    		int current_argument_offset = 2;

    		string this_brokers = this->getDefaultBrokers();
    		string this_topic = this->getDefaultTopic();
    		string this_product_code = this->getDefaultProductCode();
    		string this_hostname = this->getDefaultHostname();

    		int number_of_added_watches = 0;


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


	string LogPort::getDefaultTopic(){

		string default_topic = this->getSetting( "default.topic" );

		if( default_topic.size() > 0 ){
			return default_topic;
		}

		return "logport_logs";

	}


	string LogPort::getDefaultBrokers(){

		string default_brokers = this->getSetting( "default.brokers" );

		if( default_brokers.size() > 0 ){
			return default_brokers;
		}

		return "localhost:9092";

	}


	string LogPort::getDefaultProductCode(){

		string default_product_code = this->getSetting( "default.product_code" );

		if( default_product_code.size() > 0 ){
			return default_product_code;
		}

		return "prd000";

	}


	string LogPort::getDefaultHostname(){

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

		Observer& observer = this->getObserver();

		observer.addLogEntry( "logport: started" );


		vector<Watch> watches;

		{
			Database db;
			watches = db.getWatches();
		}

		 
		if( watches.size() == 0 ){

			observer.addLogEntry( "Started logport service with no files being watched." );

		}else{

			for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){
				Watch& watch = *it;
				//this forks for each watch
				watch.start( observer );
			}

		}


		bool have_initiated_all_stop = false;
		bool shutdown_complete = false;

		int status;

		while( !shutdown_complete ){

			//WNOHANG makes this return immediately (so we can monitor RSS)
			pid_t child_pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED );
			//pid_t child_pid = waitpid( -1, &status, WUNTRACED | WCONTINUED );


			//determine if all watches have shutdown
				if( this->run == false && shutdown_complete == false ){

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


			//determine if some watches should startup (if reload is required)
				if( this->reload_required == true && this->run == true && shutdown_complete == false && have_initiated_all_stop == false ){

					//reload from db
					{
						Database db;
						watches = db.getWatches();
					}

					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){
						Watch& watch = *it;
						if( watch.pid < 0 ){
							//not running watch; let's start it
							watch.start( observer );
						}
					}

					this->reload_required = false;

				}



			if( child_pid == -1 ){
				//error, or no child processes
                observer.addLogEntry( "logport: waitpid() error or no watches present. errno: " + logport::to_string<int>(errno) );
                shutdown_complete = true;
				this->waitUnlessEvent( 60 );
			}

			if( child_pid == 0 ){
				//no children have exited


				//stop all children
					if( this->run == false && have_initiated_all_stop == false ){

						Database db;

						observer.addLogEntry( "logport: gracefully stopping all watches..." );

						for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

							Watch& watch = *it;

							if( watch.pid > 0 ){
								watch.last_pid = watch.pid;
								
								//we're not going to use watch.stop() because it contains a shutdown delay and we want the delays to run in parallel (without needing to add threading, if possible)
								//watch.stop( observer );

						        //terminate gracefully first, then forcefully if graceful shutdown lasts longer than 20s
						        if( kill(watch.pid, SIGINT) == -1 ){
						            observer.addLogEntry( "logport: failed to kill watch with SIGINT." );
						        }
						        
								watch.pid = -1;
								watch.savePid( db );

							}

						}

						have_initiated_all_stop = true;

					}


				//ensure each watch is not using more than 250MB of memory

				if( this->run == true ){

					for( vector<Watch>::iterator it = watches.begin(); it != watches.end(); ++it ){

						Watch& watch = *it;

						if( watch.pid > 0 ){
							//if there's an existing process (ie. not the first loop iteration)

							int watch_process_rss = proc_status_get_rss_usage_in_kb( watch.pid );
							const string process_name = proc_status_get_name( watch.pid );

							observer.addLogEntry( "logport: watch process_name(" + process_name + "), RSS(" + logport::to_string<int>(watch_process_rss) + "KB), PID(" + logport::to_string<pid_t>(watch.pid) + ")" );

							if( watch_process_rss > 250000 ){ //250MB
								//kill -9
								//wait
								//respawn

								observer.addLogEntry( "logport: watch (pid: " + logport::to_string<pid_t>(watch.pid) + ", file: " + watch.watched_filepath + ") was killed because the RSS exceeded 250MB." );

								watch.last_pid = watch.pid;

								watch.stop( observer );

								sleep(5);
								
								watch.start( observer );
								if( watch.last_pid == watch.pid ){

									observer.addLogEntry( "logport: watch was killed and the new process has the same PID. Exiting." );

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
					observer.addLogEntry( "logport: Notified of last PID." );
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
					observer.addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") exited with status " + logport::to_string<int>(exit_status) );
					
					if( current_watch != NULL && this->run == true ){
						observer.addLogEntry( "restarting..." );
						current_watch->last_pid = child_pid;
						current_watch->start( observer );
					}


				}else if( WIFSIGNALED(status) ){

					int signal_number = WTERMSIG(status);
					observer.addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") killed by signal " + logport::to_string<int>(signal_number) );
					
					if( current_watch != NULL && this->run == true ){
						observer.addLogEntry( "restarting..." );
						current_watch->last_pid = child_pid;
						current_watch->start( observer );
					}


				}else if( WIFSTOPPED(status) ){

					int signal_number = WSTOPSIG(status);
					observer.addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") stopped by signal " + logport::to_string<int>(signal_number) );
					if( current_watch != NULL && this->run == true ){
						current_watch->last_pid = child_pid;
						current_watch->pid = -1;
						Database db;
						current_watch->savePid( db );
					}

				}else if( WIFCONTINUED(status) ){

					observer.addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") continued" );


				}else{

					observer.addLogEntry( "logport: PID (" + logport::to_string<pid_t>(child_pid) + ") unknown return from waitpid " + logport::to_string<int>(status) );

				}

			}

		}

		observer.addLogEntry( "logport: service shutdown complete" );

	}







}
