#include "Common.h"

#include <cstdio>
#include <stdexcept>
#include <memory>
#include <stdio.h>
#include <fstream>
#include <sstream>

#include <sys/stat.h>


#include <stdlib.h>
#include <string.h>

#include <sstream>

#include <unistd.h>

#include <cerrno>

#include <cstring>

#include <sys/time.h>

#include <sys/wait.h>

#include <cctype>
#include <algorithm>

using std::cout;
using std::cerr;
using std::endl;



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



	string execute_command( const string& command, int& exit_code ){

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
			int pclose_exit_code = pclose(pipe); 
			exit_code = WEXITSTATUS( pclose_exit_code );
			throw;
		}
		
		int pclose_exit_code = pclose(pipe); 
		exit_code = WEXITSTATUS( pclose_exit_code );

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


	uint64_t get_file_size( const string& filepath ){

		struct stat buffer;

		int stat_result = stat( filepath.c_str(), &buffer );

		if( stat_result == -1 ){
			//file does not exist
			return 0;
		}

		return buffer.st_size;

	}


	string get_real_filepath( const string& relative_filepath ){
	
		char buffer[4097];

        char *result = realpath( relative_filepath.c_str(), buffer );

        if( result == NULL ){
        	//realpath failed
			throw std::runtime_error( std::strerror(errno) );
        }

        return string(buffer);

	}


	vector<string> split_string( const string& source, char delimiter ){

		std::vector<std::string> output;
		std::istringstream buffer( source );
		std::string token;
		
		while( std::getline(buffer, token, delimiter) ) {
			output.push_back( token );
		}
		
		return output;

	}


    string to_lower( const string& source ){
        string destination = source;
        std::transform(
            source.begin(),
            source.end(),
            destination.begin(),
            [](unsigned char c){ return std::tolower(c); }
        );
        return destination;
    }



	string get_executable_filepath( const string& relative_filepath ){
	
		if( relative_filepath.size() == 0 ){
			throw std::runtime_error( "Executable path is empty." );
		}

		if( relative_filepath[0] == '/' ){
			return get_real_filepath( relative_filepath );
		}	


		char buffer[4097];

		char* system_path = getenv("PATH");
  		if( system_path == NULL ){
  			//no PATH set

  			char *result = realpath( relative_filepath.c_str(), buffer );
	        if( result == NULL ){
	        	//realpath failed
				throw std::runtime_error( std::strerror(errno) );
	        }

  		}

		string system_path_str( system_path );


		vector<string> path_entries = split_string( system_path_str, ':' );

		for( vector<string>::iterator it = path_entries.begin(); it != path_entries.end(); it++ ){

			string path_entry = *it;

			path_entry += "/" + relative_filepath;

			char *result = realpath( path_entry.c_str(), buffer );
			if( result != NULL ){
				//realpath succeeded
				return string(buffer);
			}

		}
        
        throw std::runtime_error( "Executable path not found." );

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


    string get_timestamp(){

        //get timestamp (nanoseconds)
        string current_time_string = "0.0";
        timespec current_time;
        if( clock_gettime(CLOCK_REALTIME, &current_time) == 0 ){

            char buffer[50];

            // thanks to https://stackoverflow.com/a/8304728/278976
            sprintf( buffer, "%lld.%.9ld", (long long)current_time.tv_sec, current_time.tv_nsec );
            current_time_string = string(buffer);

        }

        return current_time_string;

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

	vector<string> proc_stat_values( pid_t pid ){

	    std::ostringstream ss;
	    ss << "/proc/";
	    ss << pid;
	    ss << "/stat";

	    string stat_path = ss.str();

	    return split_string( get_file_contents(stat_path), ' ' );

	}



	       
	string get_hostname(){

		char hostname_buffer[1024];

		int result = gethostname( hostname_buffer, 1024 );

		if( result == -1 ){
			return "";
		}

		return string( hostname_buffer );

	}


    long int string_to_long( string input ){
  		return strtol( input.c_str(), NULL, 10 );
    }

    unsigned long int string_to_ulong( string input ){
  		return strtoul( input.c_str(), NULL, 10 );
    }

}








#include <cstdint>
#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>



/*
  https://github.com/superwills/NibbleAndAHalf
  base64.h -- Fast base64 encoding and decoding.
  version 1.0.0, April 17, 2013 143a
  Copyright (C) 2013 William Sherif
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  William Sherif
  will.sherif@gmail.com
  YWxsIHlvdXIgYmFzZSBhcmUgYmVsb25nIHRvIHVz
*/


const static char* b64="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;

// maps A=>0,B=>1..
const static unsigned char unb64[]={
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //10
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //20
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //30
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //40
        0,   0,   0,  62,   0,   0,   0,  63,  52,  53, //50
        54,  55,  56,  57,  58,  59,  60,  61,   0,   0, //60
        0,   0,   0,   0,   0,   0,   1,   2,   3,   4, //70
        5,   6,   7,   8,   9,  10,  11,  12,  13,  14, //80
        15,  16,  17,  18,  19,  20,  21,  22,  23,  24, //90
        25,   0,   0,   0,   0,   0,   0,  26,  27,  28, //100
        29,  30,  31,  32,  33,  34,  35,  36,  37,  38, //110
        39,  40,  41,  42,  43,  44,  45,  46,  47,  48, //120
        49,  50,  51,   0,   0,   0,   0,   0,   0,   0, //130
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //140
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //150
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //160
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //170
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //180
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //190
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //200
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //210
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //220
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //230
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //240
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //250
        0,   0,   0,   0,   0,   0,
}; // This array has 256 elements

// Converts binary data of length=len to base64 characters.
// Length of the resultant string is stored in flen
// (you must pass pointer flen).
static char* base64( const void* binaryData, int len, int *flen )
{
    const unsigned char* bin = (const unsigned char*) binaryData ;
    char* res ;

    int rc = 0 ; // result counter
    int byteNo ; // I need this after the loop

    int modulusLen = len % 3 ;
    int pad = ((modulusLen&1)<<1) + ((modulusLen&2)>>1) ; // 2 gives 1 and 1 gives 2, but 0 gives 0.

    *flen = 4*(len + pad)/3 ;
    res = (char*) malloc( *flen + 1 ) ; // and one for the null
    if( !res )
    {
        throw std::runtime_error( "ERROR: base64 could not allocate enough memory." );
        return 0;
    }

    for( byteNo = 0 ; byteNo <= len-3 ; byteNo+=3 )
    {
        unsigned char BYTE0=bin[byteNo];
        unsigned char BYTE1=bin[byteNo+1];
        unsigned char BYTE2=bin[byteNo+2];
        res[rc++]  = b64[ BYTE0 >> 2 ] ;
        res[rc++]  = b64[ ((0x3&BYTE0)<<4) + (BYTE1 >> 4) ] ;
        res[rc++]  = b64[ ((0x0f&BYTE1)<<2) + (BYTE2>>6) ] ;
        res[rc++]  = b64[ 0x3f&BYTE2 ] ;
    }

    if( pad==2 )
    {
        res[rc++] = b64[ bin[byteNo] >> 2 ] ;
        res[rc++] = b64[ (0x3&bin[byteNo])<<4 ] ;
        res[rc++] = '=';
        res[rc++] = '=';
    }
    else if( pad==1 )
    {
        res[rc++]  = b64[ bin[byteNo] >> 2 ] ;
        res[rc++]  = b64[ ((0x3&bin[byteNo])<<4)   +   (bin[byteNo+1] >> 4) ] ;
        res[rc++]  = b64[ (0x0f&bin[byteNo+1])<<2 ] ;
        res[rc++] = '=';
    }

    res[rc]=0; // NULL TERMINATOR! ;)
    return res ;
}



static unsigned char* unbase64( const char* ascii, int len, int *flen )
{
    const unsigned char *safeAsciiPtr = (const unsigned char*)ascii ;
    unsigned char *bin ;
    int cb=0;
    int charNo;
    int pad = 0 ;

    if( len < 2 ) { // 2 accesses below would be OOB.
        // catch empty string, return NULL as result.
        throw std::runtime_error( "ERROR: You passed an invalid base64 string (too short). You get NULL back." );
        *flen=0;
        return 0 ;
    }
    if( safeAsciiPtr[ len-1 ]=='=' )  ++pad ;
    if( safeAsciiPtr[ len-2 ]=='=' )  ++pad ;

    *flen = 3*len/4 - pad ;
    bin = (unsigned char*)malloc( *flen ) ;
    if( !bin )
    {
        throw std::runtime_error( "ERROR: unbase64 could not allocate enough memory." );
        return 0;
    }

    for( charNo=0; charNo <= len - 4 - pad ; charNo+=4 )
    {
        int A=unb64[safeAsciiPtr[charNo]];
        int B=unb64[safeAsciiPtr[charNo+1]];
        int C=unb64[safeAsciiPtr[charNo+2]];
        int D=unb64[safeAsciiPtr[charNo+3]];

        bin[cb++] = (A<<2) | (B>>4) ;
        bin[cb++] = (B<<4) | (C>>2) ;
        bin[cb++] = (C<<6) | (D) ;
    }

    if( pad==1 )
    {
        int A=unb64[safeAsciiPtr[charNo]];
        int B=unb64[safeAsciiPtr[charNo+1]];
        int C=unb64[safeAsciiPtr[charNo+2]];

        bin[cb++] = (A<<2) | (B>>4) ;
        bin[cb++] = (B<<4) | (C>>2) ;
    }
    else if( pad==2 )
    {
        int A=unb64[safeAsciiPtr[charNo]];
        int B=unb64[safeAsciiPtr[charNo+1]];

        bin[cb++] = (A<<2) | (B>>4) ;
    }

    return bin ;
}



namespace logport {

    string encodeBase64( const string& binary_data ){

        int result_length;
        char *result = base64( binary_data.c_str(), binary_data.size(), &result_length );

        string output( result, result_length );

        if( result ){
            free(result);
        }

        return output;

    }

    string decodeBase64( const string& ascii_data ){

        int result_length;
        unsigned char *result = unbase64( ascii_data.c_str(), ascii_data.size(), &result_length );

        string output( reinterpret_cast<char*>(result), result_length );

        if( result ){
            free(result);
        }

        return output;

    }

}

