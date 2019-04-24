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


#include <map>
using std::map;


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
		execute_command( "rmdir /usr/local/logport" );

		cout << "Run these commands to finalize the uninstall:" << endl;
		cout << "rm /usr/local/lib/logport/librdkafka.so.1" << endl;
		cout << "rmdir /usr/local/lib/logport" << endl;
		cout << "rm /usr/local/bin/logport" << endl;

	}


	void LogPort::start(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl start logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport start" );
			return;
		}

		this->printUnsupportedPlatform();
		
	}


	
	void LogPort::stop(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl stop logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport stop" );
			return;
		}

		this->printUnsupportedPlatform();

	}


	void LogPort::restart(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl restart logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport restart" );
			return;
		}

		this->printUnsupportedPlatform();

	}


	void LogPort::reload(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl reload logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport reload" );
			return;
		}

		this->printUnsupportedPlatform();

	}

	void LogPort::status(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl status logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport status" );
			return;
		}

		this->printUnsupportedPlatform();

	}



	void LogPort::enable(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl enable logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
			cout << execute_command( "chkconfig logport on" );
			return;
		}

		this->printUnsupportedPlatform();

	}


	void LogPort::disable(){

		this->current_platform.determinePlatform();

		if( this->current_platform.os.name == "ubuntu" ){
			cout << execute_command( "systemctl disable logport.service" );
			return;
		}

		if( this->current_platform.os.name == "oel" ){
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
"\n"
"manage settings\n"
"   set        Set a setting's value\n"
"   unset      Clear a setting's value\n"
"   settings   List all settings\n"
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

	    		if( this->command == "now" ){
	    			this->watchNow( watch );
	    		}else{
	    			this->addWatch( watch );
	    		}

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
		column_labels.push_back( "file_offset_sent" );

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

				// file_offset
					std::ostringstream file_offset_stringstream;
					file_offset_stringstream << watch.file_offset;
					string file_offset_string = file_offset_stringstream.str();

					if( file_offset_string.size() > column_widths_maximums[4] ){
						column_widths_maximums[4] = file_offset_string.size();
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
			cout << std::right << std::setw(column_widths_maximums[4]) << watch.file_offset << endl;

		}

	}



	void LogPort::addWatch( const Watch& watch ){

		Database& db = this->getDatabase();

		PreparedStatement statement( db, "INSERT INTO watches ( filepath, file_offset, brokers, topic ) VALUES ( ?, ?, ?, ? );" );

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




	void LogPort::watchNow( const Watch& watch ) const{

		KafkaProducer kafka_producer( watch.brokers, watch.topic, watch.undelivered_log_filepath );  

		InotifyWatcher watcher( watch.watched_filepath, watch.undelivered_log_filepath, kafka_producer );  //expects undelivered log to exist
		//inotify_watcher_ptr = &watcher;

		watcher.watch(); //main loop; blocks

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



}
