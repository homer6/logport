#ifndef LOGPORT_INSPECTOR_H
#define LOGPORT_INSPECTOR_H

#include <string>
using std::string;



#include <fstream>


namespace logport{

	class Inspector{

	    public:
	    	Inspector();
	    	~Inspector();

	    	void monitorTwoSecondsTick();
	    	void monitorTenSecondsTick();
	    	void monitorDayTick();

	    	void produceTelemetryReadingFromFile( const string& filepath );
	    	void produceTelemetryReadingFromCommand( const string& command );

	    	void rotateLog();

	    private:
	    	std::ofstream telemetry_file;

	};

}




#endif //LOGPORT_INSPECTOR_H
