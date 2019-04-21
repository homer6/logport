#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;




namespace logport{

	class Watch;
	class Database;

	class LogPort{

	    public:
	    	LogPort();
	    	~LogPort();

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


	        void printHelpWatch();


	        int runFromCommandLine( int argc, char **argv );
	        void registerSignalHandlers();

	        void addWatch( const Watch& watch );
	        void listWatches();

	        Database& getDatabase();


	        string getDefaultTopic();
	        string getDefaultBrokers();

	        string executeCommand( const string& command );
	        bool fileExists( const string& filename );

	    protected:
	    	void installInitScript();

	    private:
	    	Database *db;


	    public:
	     	bool run;
	     	string command;
	     	vector<string> command_line_arguments;
	     	string current_version;


	};

}




#endif //LOGPORT_LOGPORT_H
