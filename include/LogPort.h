#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "Platform.h"


namespace logport{

	class Watch;
	class Database;

	class Inspector;


	class LogPort{

	    public:
	    	LogPort();
	    	~LogPort();

	    	void closeDatabase();
	    	
	        void install();
	        void uninstall();
	        void destroy();
	        void restoreToFactoryDefault();
	        void enable();
	        void disable();
	        

	        void start();
	        void stop();
	        void restart();
	        void reload();
	        void status();

	        bool isRunning();



	        void printHelp();
	        void printVersion();


	        void printHelpWatch();
	        void printHelpSet();
	        void printHelpUnset();
	        void printHelpInspect();

	        void printUnsupportedPlatform();


	        int runFromCommandLine( int argc, char **argv );
	        void registerSignalHandlers();

	        void addWatch( const Watch& watch );
	        void listWatches();

			void watchNow( const Watch& watch ) const;


			bool addSetting( const string& key, const string& value );
			string getSetting( const string& key );
			bool removeSetting( const string& key );
			void listSettings();


	        Database& getDatabase();
	        Inspector& getInspector();


	        string getDefaultTopic();
	        string getDefaultBrokers();
	        string getDefaultProductCode();


	    protected:
	    	void installInitScript();

	    	void startWatches();  //main loop (blocks)



	    private:
	    	Database *db;
	    	Inspector* inspector;

	    public:
	     	bool run;
	     	string command;
	     	vector<string> command_line_arguments;
	     	string current_version;
	     	Platform current_platform;

	     	string pid_filename;
	     	

	};

}




#endif //LOGPORT_LOGPORT_H
