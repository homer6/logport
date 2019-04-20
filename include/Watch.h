#ifndef LOGPORT_WATCH_H
#define LOGPORT_WATCH_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <stdint.h>

namespace logport{

	class Watch{

	    public:
	    	Watch();
	    	Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, int64_t file_offset = 0 );

	        string watched_filepath;  			//eg. "/var/log/syslog"
	        string undelivered_log_filepath;  	//eg. "/var/log/syslog_undelivered.log"

	        string brokers;  					//csv separated
	        string topic;						//eg. "my_logs"

	        int64_t file_offset;


	};

}



#endif //LOGPORT_WATCH_H
