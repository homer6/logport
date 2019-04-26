#ifndef LOGPORT_PLATFORM_H
#define LOGPORT_PLATFORM_H

#include <string>
using std::string;



namespace logport{


	class Version{

	    public:
	    	Version();

	     	string name;
	     	int major_version;
	     	int minor_version;
	     	int patch_version;

	};



	class Platform{

	    public:
	    	void determinePlatform();

	     	Version os;
	     	Version kernel;

	     	string service_manager;
	     	

	};

}




#endif //LOGPORT_PLATFORM_H
