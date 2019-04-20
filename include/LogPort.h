#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;




namespace logport{

	class Watch;

	class LogPort{

	    public:
	    	LogPort();

	        void ensureInstalled();
	        void install();
	        void uninstall();

	        void start();
	        void stop();
	        void restart();
	        void reload();
	        void status();

	        void printHelp();
	        void printVersion();

	        int runFromCommandLine( int argc, char **argv );
	        void registerSignalHandlers();

	        void addWatch( const Watch& watch );


	        string getDefaultTopic();
	        string getDefaultBrokers();

	        string executeCommand( const string& command );

	    protected:
	    	void installInitScript();


	    public:
	     	bool run;
	     	string command;
	     	vector<string> command_line_arguments;
	     	string current_version;


	};

}




#endif //LOGPORT_LOGPORT_H
