#include "Database.h"

#include "sqlite3.h"
#include <stdexcept>

#include <string>
using std::string;

#include "Watch.h"
#include "PreparedStatement.h"

#include <unistd.h>


namespace logport{

    Database::Database(){

        this->db = NULL;

        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_open_v2( "/usr/local/logport/logport.db", &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }
         

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
                "topic TEXT, "
                "product_code TEXT, "
                "hostname TEXT, "
                "pid INTEGER DEFAULT -1 "
            ")"
        );

        this->execute( "CREATE TABLE settings ( "
                "key TEXT PRIMARY KEY, "
                "value TEXT "
            ")"
        );

    }


    void Database::execute( const string& command ){

        char *error_message = 0;

        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_exec( this->db, command.c_str(), NULL, 0, &error_message );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }

        if( result_code != SQLITE_OK ){

            string error_message_string( error_message );
            sqlite3_free( error_message );

            throw std::runtime_error( "Sqlite: " + error_message_string );
            
        }

    }


    vector<Watch> Database::getWatches(){

        PreparedStatement statement( *this, "SELECT * FROM watches;" );

        vector<Watch> watches;

        while( statement.step() == SQLITE_ROW ){

            Watch watch(statement);

            watches.push_back( watch );
            
        }

        return watches;

    }



    Watch Database::getWatchByPid( pid_t pid ){

        PreparedStatement statement( *this, "SELECT * FROM watches WHERE pid = ? ;" );
        statement.bindInt32( 0, pid );

        while( statement.step() == SQLITE_ROW ){

            return Watch(statement);
            
        }

        throw std::runtime_error( "Watch pid not found." );

    }



    map<string,string> Database::getSettings(){

        PreparedStatement statement( *this, "SELECT * FROM settings;" );

        map<string,string> settings;

        while( statement.step() == SQLITE_ROW ){

            string key = statement.getText(0);
            string value = statement.getText(1);

            settings[key] = value;
            
        }

        return settings;

    }



    string Database::getSetting( const string& key ){

        PreparedStatement statement( *this, "SELECT * FROM settings WHERE key = ?;" );
        statement.bindText( 0, key );

        string value;

        while( statement.step() == SQLITE_ROW ){
            value = statement.getText(1);
        }

        return value;

    }


}
