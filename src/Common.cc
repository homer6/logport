#include "Common.h"

#include <cstdio>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <fstream>
#include <sstream>

#include <sys/stat.h>


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



}




