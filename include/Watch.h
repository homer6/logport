#ifndef LOGPORT_WATCH_H
#define LOGPORT_WATCH_H

#include <string>
using std::string;

#include <vector>
using std::vector;


namespace logport{

	class Watch{

	    public:
	    	Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic );


	        string watched_filepath;  			//eg. "/var/log/syslog"
	        string undelivered_log_filepath;  	//eg. "/var/log/syslog_undelivered.log"

	        string brokers;  					//csv separated
	        string topic;						//eg. "my_logs"


	};

}



#endif //LOGPORT_WATCH_H
