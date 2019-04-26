#ifndef LOGPORT_DATABASE_H
#define LOGPORT_DATABASE_H

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <sys/types.h>


class sqlite3;

namespace logport{

	class Watch;
	class PreparedStatement;

	class Database{

	    public:
	    	Database();
	    	~Database();

	    	void createDatabase();

	    	vector<Watch> getWatches();
	    	Watch getWatchByPid( pid_t pid );

	    	string getSetting( const string& key );
	    	map<string,string> getSettings();

	    	void execute( const string& command );

	    private:
	    	sqlite3 *db;

	    friend class PreparedStatement;

	};





}



#endif //LOGPORT_DATABASE_H
