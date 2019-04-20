#ifndef LOGPORT_DATABASE_H
#define LOGPORT_DATABASE_H

#include <string>
using std::string;

#include <vector>
using std::vector;

class sqlite3;

namespace logport{

	class Watch;
	class PreparedStatement;

	class Database{

	    public:
	    	Database();
	    	~Database();

	    	void createDatabase();

	    	void execute( const string& command );

	    	void addWatch( const Watch& watch );

	    private:
	    	sqlite3 *db;

	    friend class PreparedStatement;

	};





}



#endif //LOGPORT_DATABASE_H
