#ifndef LOGPORT_DATABASE_H
#define LOGPORT_DATABASE_H

#include <string>
using std::string;

#include <vector>
using std::vector;

class sqlite3;

namespace logport{

	class Database{

	    public:
	    	Database();

	    	static bool databaseExists();
	    	static void createDatabase();

	    private:
	    	sqlite3 *db;

	};

}



#endif //LOGPORT_DATABASE_H
