#ifndef LOGPORT_COMMON_H
#define LOGPORT_COMMON_H

#include <string>
using std::string;

#include <stdint.h>
#include <sys/types.h>



namespace logport{

	string execute_command( const string& command );

	bool file_exists( const string& filename );

	string get_file_contents( const string& filepath);

	//gets the absolute path (resolves symlinks too)
	string get_real_filepath( const string& relative_filepath );


	string escape_to_json_string( const string& unescaped_string );




	// resource usage

	int proc_status_get_rss_usage_in_kb( pid_t pid );

	string proc_status_get_name( pid_t pid );


	// computer identification

    string get_hostname();



}




#endif //LOGPORT_COMMON_H
