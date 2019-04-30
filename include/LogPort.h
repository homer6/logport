#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "Platform.h"
#include "Observer.h"


namespace logport{

	class Watch;
	class Database;

	class Inspector;


	class LogPort{

	    public:
	    	LogPort();
	    	~LogPort();

	    	void closeDatabase();
	    	void closeObserver();
	    	
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
	        void reloadIfRunning();
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
	        Observer& getObserver();


	        string getDefaultTopic();
	        string getDefaultBrokers();
	        string getDefaultProductCode();
	        string getDefaultHostname();


	    protected:
	    	void installInitScript();
	    	void installLogrotate();

	    	void startWatches();  //main loop (blocks)


	    	//this will wait for 60 seconds; but, it'll check to see if there's an event every second
	    	void waitUnlessEvent( int seconds );



	    private:
	    	Database *db;
	    	Inspector* inspector;
	    	Observer* observer;

	    public:
	     	bool run;
	     	bool reload_required;
	     	string command;
	     	vector<string> command_line_arguments;
	     	string current_version;
	     	Platform current_platform;

	     	string pid_filename;

	     	bool verbose_mode;

	     	

	};

}




#endif //LOGPORT_LOGPORT_H
