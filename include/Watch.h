#ifndef LOGPORT_WATCH_H
#define LOGPORT_WATCH_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <stdint.h>
#include <sys/types.h>

#include <fstream>

#include "Producer.h"
#include "UrlList.h"

namespace logport{

	class PreparedStatement;
	class Database;
	class LogPort;

	class Watch{

	    public:
	    	Watch();

            /**
             * Detects the ProducerType based on the Url schemes.
             * Defaults to ProducerType::KAFKA. Selects ProducerType::HTTP if "http" or "https" scheme is used.
             * @throws if schemes do not match
             * @param brokers comma-separated list of Urls (ordered retained)
             */
            Watch( const string& brokers );

	    	Watch( const PreparedStatement& statement );
	    	Watch(
                const string& watched_filepath,
                const string& undelivered_log_filepath,
                const string& brokers,
                const string& topic,
                const string& product_code,
                const string& log_type,
                const string& hostname,
                int64_t file_offset = 0,
                pid_t pid = -1
            );


            void setBrokers( const string& brokers );
            void setProducerType( ProducerType producer_type );




	        string watched_filepath;  			//eg. "/var/log/syslog"
	        string undelivered_log_filepath;  	//eg. "/var/log/syslog_undelivered.log"

            ProducerType producer_type = ProducerType::KAFKA;
            string producer_type_description = "KAFKA";

            string brokers;  					//csv separated (include http path for http producer)
	        string topic;						//eg. "my_logs" (unused for http producer)
	        string product_code;				//eg. "prd123"
	        string log_type;				    //eg. "system"
	        string hostname;				    //eg. "my.hostname.com"

            homer6::UrlList brokers_url_list;

	        int64_t id;	        
	        int64_t file_offset;
	        int64_t last_undelivered_size;

	        pid_t pid;
	        pid_t last_pid;

	    	pid_t start( LogPort* logport );
            void runNow( LogPort* logport ); //blocks; runs in current process
	    	void stop( LogPort* logport );

	    	void savePid( Database& db );
	    	void saveOffset( Database& db );
	    	void loadOffset( Database& db );

	        void bind( PreparedStatement& statement, bool skip_id = true ) const;

	        string filterLogLine( const string& unfiltered_log_line ) const;

	};

}



#endif //LOGPORT_WATCH_H
