#include "Inspector.h"

#include "Common.h"

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;


namespace logport{

	Inspector::Inspector(){

		this->telemetry_file.open( "/usr/local/logport/telemetry.log", std::ofstream::out | std::ofstream::app );

	}

	Inspector::~Inspector(){

		this->telemetry_file.close();

	}

	void Inspector::rotateLog(){

		this->telemetry_file.close();
		this->telemetry_file.open( "/usr/local/logport/telemetry.log", std::ofstream::out | std::ofstream::app );

	}


	// https://www.cyberciti.biz/tips/top-linux-monitoring-tools.html
	// https://www.cyberciti.biz/files/linux-kernel/Documentation/filesystems/proc.txt

	void Inspector::monitorTwoSecondsTick(){

		this->produceTelemetryReadingFromCommand( "ps aux" );

	}

	void Inspector::monitorTenSecondsTick(){

		this->produceTelemetryReadingFromFile( "/proc/meminfo" );
		this->produceTelemetryReadingFromFile( "/proc/stat" );
		this->produceTelemetryReadingFromFile( "/proc/net/netstat" );
		this->produceTelemetryReadingFromFile( "/proc/net/sockstat" );

	}

	void Inspector::monitorDayTick(){

		this->produceTelemetryReadingFromFile( "/proc/cpuinfo" );

	}


	void Inspector::produceTelemetryReadingFromFile( const string& filepath ){

		string telemetry_reading = escape_to_json_string( get_file_contents( filepath.c_str() ) );

		this->telemetry_file << "{\"source\":\"" + escape_to_json_string(filepath) + "\",\"contents\":\"" + telemetry_reading + "\"}" << endl;

	}

	void Inspector::produceTelemetryReadingFromCommand( const string& command ){

		string telemetry_reading = escape_to_json_string( execute_command( command.c_str() ) );

		this->telemetry_file << "{\"source\":\"" + escape_to_json_string(command) + "\",\"contents\":\"" + telemetry_reading + "\"}" << endl;

	}


}
