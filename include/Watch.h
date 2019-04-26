#ifndef LOGPORT_WATCH_H
#define LOGPORT_WATCH_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <stdint.h>
#include <sys/types.h>

#include <fstream>

namespace logport{

	class PreparedStatement;
	class Database;

	class Watch{

	    public:
	    	Watch();
	    	Watch( const PreparedStatement& statement );
	    	Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, int64_t file_offset = 0, pid_t pid = -1 );

	        string watched_filepath;  			//eg. "/var/log/syslog"
	        string undelivered_log_filepath;  	//eg. "/var/log/syslog_undelivered.log"

	        string brokers;  					//csv separated
	        string topic;						//eg. "my_logs"

	        int64_t id;	        
	        int64_t file_offset;

	        pid_t pid;
	        pid_t last_pid;


	    	
	    	pid_t start( std::ofstream& log_file );
	    	void stop( std::ofstream& log_file );


	    	void savePid( Database& db );
	    	void saveOffset( Database& db );

	        void bind( PreparedStatement& statement, bool skip_id = true ) const;


	};

}



#endif //LOGPORT_WATCH_H
