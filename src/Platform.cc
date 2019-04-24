#include "Platform.h"

#include "Common.h"

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;


namespace logport{

	Version::Version()
		:name("unknown"), major_version(-1), minor_version(-1), patch_version(-1)
	{

	
	
	}

	void Platform::determinePlatform(){

		string issue_contents = get_file_contents( "/etc/issue" );

		//string uname_contents = execute_command( "uname -a" );

		//cout << issue_contents << endl;
		//cout << uname_contents << endl;

		std::size_t found;


		found = issue_contents.find( "Oracle Linux Server" );
  		if( found != std::string::npos ){
  			this->os.name = "oel";

			found = issue_contents.find( "Oracle Linux Server release 5." );
	  		if( found != std::string::npos ){
	  			this->os.major_version = 5;
	  		}

	  		return;
  		}


		found = issue_contents.find( "Ubuntu" );
  		if( found != std::string::npos ){
  			this->os.name = "ubuntu";

			found = issue_contents.find( "Ubuntu 18." );
	  		if( found != std::string::npos ){
	  			this->os.major_version = 18;
	  		}

	  		return;
  		}
    

	}


}
