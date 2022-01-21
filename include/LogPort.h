#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

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
	        void printHelpAdopt();

	        void printUnsupportedPlatform();


	        int runFromCommandLine( int argc, char **argv );
	        void registerSignalHandlers();

	        void addWatch( const Watch& watch );
	        void listWatches();

			void watchNow( Watch& watch );
			void adopt( const Watch& watch );


			bool addSetting( const string& key, const string& value );
			string getSetting( const string& key );
			bool removeSetting( const string& key );
			void listSettings();

			void loadEnvironmentVariables();
            string getEnvironmentVariable( const string& variable_name ) const;
            void setEnvironmentVariable( const string& variable_name, const string& variable_value );

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


	    	
	    	pid_t startProcess( const string& executable_path, const string& process_description, const std::vector<string>& arguments, const std::map<string,string>& environment_variables, int *child_stdin_pipe, int *child_stdout_pipe, int *child_stderr_pipe );


	    private:
	    	Database *db;
	    	Inspector* inspector;
	    	Observer* observer;

	    public:
	     	bool run;
	     	bool reload_required;
	     	bool watches_paused;
	     	string command;
	     	vector<string> command_line_arguments;
	     	string current_version;
	     	Platform current_platform;

	     	vector<string> additional_arguments;
	     	map<string,string> environment_variables;


	     	string pid_filename;

	     	bool verbose_mode;

	     	

	};

}




#endif //LOGPORT_LOGPORT_H
