#ifndef LOGPORT_COMMON_H
#define LOGPORT_COMMON_H

#include <string>
using std::string;

#include <stdint.h>
#include <sys/types.h>

#include <sstream>


namespace logport{

	string execute_command( const string& command );

	bool file_exists( const string& filename );

	string get_file_contents( const string& filepath);

	uint64_t get_file_size( const string& filepath );

	//gets the absolute path (resolves symlinks too)
	string get_real_filepath( const string& relative_filepath );


	string escape_to_json_string( const string& unescaped_string );


	//format: 1556311722.644052770
	string get_timestamp();


	// resource usage

	int proc_status_get_rss_usage_in_kb( pid_t pid );

	string proc_status_get_name( pid_t pid );


	// computer identification

    string get_hostname();



    template<class T> 
    string to_string( T input ){
		std::ostringstream stringstream;
		stringstream << input;
		return stringstream.str();
    };


}




#endif //LOGPORT_COMMON_H
