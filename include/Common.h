#ifndef LOGPORT_COMMON_H
#define LOGPORT_COMMON_H

#include <string>
using std::string;



namespace logport{

	string execute_command( const string& command );

	bool file_exists( const string& filename );

	string get_file_contents( const string& filepath);

	string escape_to_json_string( const string& unescaped_string );

}




#endif //LOGPORT_COMMON_H
