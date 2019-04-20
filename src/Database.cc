#include "Database.h"

#include <sqlite3.h>
#include <stdexcept>

#include <string>
using std::string;

#include "Watch.h"
#include "PreparedStatement.h"


namespace logport{

    Database::Database(){

        this->db = NULL;

        int result_code = sqlite3_open_v2( "/usr/local/logport/logport.db", &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );

        if( result_code != SQLITE_OK ){

            if( this->db != NULL ){

                string error_message( sqlite3_errmsg(this->db) );
                sqlite3_close( this->db );

                throw std::runtime_error( "Sqlite: " + error_message );

            }

            throw std::runtime_error( "Sqlite: could not create connection (out of memory)." );

        }


    }    


    Database::~Database(){

        sqlite3_close( this->db );

    }


    void Database::createDatabase(){

        this->execute( "CREATE TABLE watches ( "
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "filepath TEXT NOT NULL, "
                "file_offset INTEGER DEFAULT 0, "
                "brokers TEXT, "
                "topic TEXT "
            ")"
        );

    }


    void Database::execute( const string& command ){

        char *error_message = 0;

        int result_code = sqlite3_exec( this->db, command.c_str(), NULL, 0, &error_message );
        if( result_code != SQLITE_OK ){

            string error_message_string( error_message );
            sqlite3_free( error_message );

            throw std::runtime_error( "Sqlite: " + error_message_string );
            
        }

    }



    void Database::addWatch( const Watch& watch ){




    }




}
