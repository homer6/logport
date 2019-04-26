#include "Common.h"

#include <cstdio>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <fstream>
#include <sstream>

#include <sys/stat.h>


#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <sstream>




namespace logport{



	string execute_command( const string& command ){

		// http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
		char buffer[4096];

		string output;

		FILE* pipe = popen( command.c_str(), "r" );
		if( !pipe ){
			throw std::runtime_error( "popen() failed" );
		}

		try {
			while( fgets(buffer, 4096, pipe) != NULL ){
				output += buffer;
			}
		}catch(...){
			pclose( pipe );
			throw;
		}
		pclose( pipe );

		return output;

	}


	bool file_exists( const string& filename ){
		//https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
		struct stat buffer;
		return ( stat(filename.c_str(), &buffer) == 0 ); 
	}


	string get_file_contents( const string& filepath ){

		std::ifstream text_file( filepath.c_str() );
		std::stringstream buffer;
		buffer << text_file.rdbuf();
		return buffer.str();

	}



    string escape_to_json_string( const string& unescaped_string ){

        string escaped_string;

        for( std::string::size_type x = 0; x < unescaped_string.size(); ++x ){

            char current_character = unescaped_string[x];

            switch( current_character ){
                case 92: escaped_string += "\\\\"; break;       //Backslash is replaced with \\ string
                case 8: escaped_string += "\\b"; break;         //Backspace is replaced with \b
                case 12: escaped_string += "\\f"; break;        //Form feed is replaced with \f
                case 10: escaped_string += "\\n"; break;        //Newline is replaced with \n
                case 13: escaped_string += "\\r"; break;        //Carriage return is replaced with \r
                case 9: escaped_string += "\\t"; break;         //Tab is replaced with \t
                case 34: escaped_string += "\\\""; break;       //Double quote is replaced with \"
                default: escaped_string += current_character;
            };

        }

        return escaped_string;

    }





	static int parse_int_kb_line(char* line){
	    // This assumes that a digit will be found and the line ends in " kB".
	    int i = strlen(line);
	    const char* p = line;
	    while (*p <'0' || *p > '9') p++;
	    line[i-3] = '\0';
	    i = atoi(p);
	    return i;
	}

	static int proc_status_get_int_kb_value( pid_t pid, const string key ){ //Note: this value is in KB!

	    std::ostringstream ss;
	    ss << "/proc/";
	    ss << pid;
	    ss << "/status";

		string status_path = ss.str();

	    FILE* file = fopen( status_path.c_str(), "r");
	    if( !file ){
	        return -1;
	    }

	    int result = -1;
	    char line[300];

	    while( fgets(line, 256, file) != NULL ){
	        if( strncmp(line, key.c_str(), key.size()) == 0 ){
	            result = parse_int_kb_line(line);
	            break;
	        }
	    }
	    fclose(file);
	    return result;

	}





	static string parse_string_line( int prefix_offset, char* line ){

	    int line_length = strlen(line);

	    if( prefix_offset >= line_length ){
	        return "";
	    }

	    const char* start_offset = line + prefix_offset;
	    const char* end_line_offset = line + line_length;  //one past the end
	    const char* end_string_offset;

	    while( *start_offset == ' ' || *start_offset == '\t' ){        
	        start_offset++;        
	        //don't read off of the end of the line
	        if( start_offset == end_line_offset ){
	            return "";
	        }
	    }

	    end_string_offset = start_offset;

	    while( *end_string_offset != ' ' && *end_string_offset != '\n' ){        
	        end_string_offset++;        
	        //don't read off of the end of the line
	        if( start_offset == end_line_offset ){
	            return "";
	        }
	    }

	    return string(start_offset, end_string_offset - start_offset);

	}



	static string proc_status_get_string_value( pid_t pid, const string key ){

	    std::ostringstream ss;
	    ss << "/proc/";
	    ss << pid;
	    ss << "/status";

	    string status_path = ss.str();

	    FILE* file = fopen( status_path.c_str(), "r");
	    if( !file ){
	        return "";
	    }

	    try{
	        string result;
	        char line[300];

	        while( fgets(line, 256, file) != NULL ){
	            if( strncmp(line, key.c_str(), key.size()) == 0 ){
	                result = parse_string_line(key.size(), line);
	                break;
	            }
	        }
	        fclose(file);
	        return result;

	    }catch( std::exception &e ){        
	        fclose(file);
	        throw e;
	    }

	}



	// see: https://stackoverflow.com/a/64166/278976

	int proc_status_get_rss_usage_in_kb( pid_t pid ){
		return proc_status_get_int_kb_value( pid, "VmRSS:" );
	}

	string proc_status_get_name( pid_t pid ){
	    return proc_status_get_string_value( pid, "Name:" );
	}



}




