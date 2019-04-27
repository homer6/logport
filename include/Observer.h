#ifndef LOGPORT_OBSERVER_H
#define LOGPORT_OBSERVER_H

#include <string>
using std::string;

#include <fstream>


namespace logport{


	/*
		An observer is like a typical "logger" class that can be passed around to different 
		objects and give them convenience functions for adding log entries (eg. adds a 
		timestamp) to each log entry.

		It's called an observer because, in keeping with the logport/jetstream paradigm, it
		handles all elements of observability (metrics, events, tracing, telemetry, and 
		logging). We call this collection METTL, after the first letters of this set.

	*/
	class Observer{

	    public:
	    	Observer();
	    	~Observer();

	    	void addMetricEntry( const string& metric_entry );
	    	void addEventEntry( const string& event_entry );
	    	void addTraceEntry( const string& trace_entry );
	    	void addTelemetryEntry( const string& telemetry_entry );
	    	void addLogEntry( const string& log_line );
	    	
	    protected:
	    	std::ofstream metrics_file;
	    	std::ofstream events_file;
	    	std::ofstream traces_file;
	    	std::ofstream telemetry_file;
	     	std::ofstream log_file;
	     	
	};


}




#endif //LOGPORT_OBSERVER_H
