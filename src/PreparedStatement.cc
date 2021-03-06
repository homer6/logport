#include "PreparedStatement.h"
#include "Database.h"

#include "sqlite3.h"
#include <stdexcept>

#include <string>
using std::string;

#include <unistd.h>



namespace logport{

    PreparedStatement::PreparedStatement( const Database& database, const string& statement_sql ){

        this->statement = NULL;
        this->db = database.db;
        this->last_step_result = SQLITE_ERROR;
        this->column_count = 0;


        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_prepare_v3( this->db, statement_sql.c_str(), static_cast<int>(statement_sql.size()), 0, &this->statement, NULL );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }

        if( result_code != SQLITE_OK ){

            string error_message( sqlite3_errmsg(this->db) );

            if( this->statement != NULL ){
                
                sqlite3_close( this->db );

                throw std::runtime_error( "Sqlite: " + error_message );

            }

            throw std::runtime_error( "Sqlite: could not create prepared statement: " + error_message );

        }


    }    


    PreparedStatement::~PreparedStatement(){

        // https://sqlite.org/c3ref/finalize.html

        sqlite3_finalize( this->statement );

    }



    void PreparedStatement::bindInt32( int offset, const int32_t value ){

        //The leftmost SQL parameter has an index of 1 in the sqlite API
        //We're changing that to be 0 so that it's consistent with the column family functions
        //To make this adaption, ensure that we add 1 to the provided offset.

        // http://www.sqlite.org/c3ref/bind_blob.html

        int result_code = sqlite3_bind_int( this->statement, offset + 1, value );

        if( result_code != SQLITE_OK ){

            string error_message_string( sqlite3_errmsg( this->db ) );

            throw std::runtime_error( "Sqlite bindInt32 error: " + error_message_string );

        }

    }


    void PreparedStatement::bindInt64( int offset, const int64_t value ){

        //The leftmost SQL parameter has an index of 1 in the sqlite API
        //We're changing that to be 0 so that it's consistent with the column family functions
        //To make this adaption, ensure that we add 1 to the provided offset.

        // http://www.sqlite.org/c3ref/bind_blob.html

        int result_code = sqlite3_bind_int64( this->statement, offset + 1, value );

        if( result_code != SQLITE_OK ){

            string error_message_string( sqlite3_errmsg( this->db ) );

            throw std::runtime_error( "Sqlite bindInt64 error: " + error_message_string );

        }

    }


    void PreparedStatement::bindText( int offset, const string& text ){

        //The leftmost SQL parameter has an index of 1 in the sqlite API
        //We're changing that to be 0 so that it's consistent with the column family functions
        //To make this adaption, ensure that we add 1 to the provided offset.

        // http://www.sqlite.org/c3ref/bind_blob.html

        if( text.size() == 0 ){
            return;
        }

        int result_code = sqlite3_bind_text( this->statement, offset + 1, text.c_str(), static_cast<int>(text.size()), SQLITE_TRANSIENT );

        if( result_code != SQLITE_OK ){

            string error_message_string( sqlite3_errmsg( this->db ) );

            throw std::runtime_error( "Sqlite bindText error: " + error_message_string );

        }

    }


    int PreparedStatement::step(){

        // https://sqlite.org/c3ref/step.html

        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_step( this->statement );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }

        this->last_step_result = result_code;

        if( result_code == SQLITE_ERROR ){

            string error_message_string( sqlite3_errmsg( this->db ) );

            throw std::runtime_error( "Sqlite step error: " + error_message_string );
            
        }

        if( result_code == SQLITE_ROW ){

            //store the number of columns so we can validate it on the `column` family of methods
            this->column_count = sqlite3_column_count( this->statement );

        }

        return result_code;

    }



    int PreparedStatement::reset(){

        // https://sqlite.org/c3ref/reset.html

        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_reset( this->statement );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }

        this->last_step_result = SQLITE_ERROR;
        this->column_count = 0;
        
        if( result_code != SQLITE_OK ){

            string error_message( sqlite3_errmsg(this->db) );

            throw std::runtime_error( "Sqlite reset error: " + error_message );
            
        }

        return result_code;

    }


    int PreparedStatement::clearBindings(){

        // https://sqlite.org/c3ref/clear_bindings.html

        int attempt = 0;
        int result_code;

        while( attempt < 1000 ){

            result_code = sqlite3_clear_bindings( this->statement );
            
            if( result_code != SQLITE_BUSY && result_code != SQLITE_LOCKED ){
                break;                
            }
            usleep(10000);  // 10ms
            attempt++;

        }


        if( result_code != SQLITE_OK ){

            string error_message( sqlite3_errmsg(this->db) );

            throw std::runtime_error( "Sqlite clearBindings error: " + error_message );
            
        }

        return result_code;

    }


    int32_t PreparedStatement::getInt32( int offset ) const{

        // https://sqlite.org/c3ref/column_blob.html

        // first column offset is 0

        if( this->last_step_result != SQLITE_ROW ){
            throw std::runtime_error( "Sqlite getInt32 error: last step call did not return SQLITE_ROW" );
        }

        this->validateOffset( offset );

        // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, or SQLITE_NULL
        int column_type = sqlite3_column_type( this->statement, offset );
        if( column_type != SQLITE_INTEGER ){
            throw std::runtime_error( "Sqlite getInt32 error: expected int type column, but got type: " + this->describeColumnType(column_type) );
        }

        return sqlite3_column_int( this->statement, offset );

    }


    
    int64_t PreparedStatement::getInt64( int offset ) const{

        // https://sqlite.org/c3ref/column_blob.html

        // first column offset is 0

        if( this->last_step_result != SQLITE_ROW ){
            throw std::runtime_error( "Sqlite getInt64 error: last step call did not return SQLITE_ROW" );
        }

        this->validateOffset( offset );

        // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, or SQLITE_NULL
        int column_type = sqlite3_column_type( this->statement, offset );
        if( column_type != SQLITE_INTEGER ){
            throw std::runtime_error( "Sqlite getInt64 error: expected int type column, but got type: " + this->describeColumnType(column_type) );
        }

        return sqlite3_column_int64( this->statement, offset );

    }


    string PreparedStatement::getText( int offset ) const{

        // https://sqlite.org/c3ref/column_blob.html

        // first column offset is 0

        if( this->last_step_result != SQLITE_ROW ){
            throw std::runtime_error( "Sqlite getText error: last step call did not return SQLITE_ROW" );
        }

        this->validateOffset( offset );

        // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, or SQLITE_NULL
        int column_type = sqlite3_column_type( this->statement, offset );
        if( column_type != SQLITE_TEXT ){
            throw std::runtime_error( "Sqlite getText error: expected text type column, but got type: " + this->describeColumnType(column_type) );
        }

        const unsigned char *result_text_ptr = sqlite3_column_text( this->statement, offset );

        if( result_text_ptr == NULL ){
            return string();
            //throw std::runtime_error( "Sqlite getText error: expect text but got NULL" );
        }

        int number_of_bytes = sqlite3_column_bytes( this->statement, offset );

        if( number_of_bytes == 0 ){
            return string();
        }

        string result_text( reinterpret_cast<const char *>(result_text_ptr), number_of_bytes );

        return result_text;

    }



    void PreparedStatement::validateOffset( int offset ) const{

        if( offset < 0 ){
            throw std::runtime_error( "Offset must be non-negative." );
        }

        if( offset >= this->column_count ){
            throw std::runtime_error( "Column offset does not exist." );
        }

    }


    int PreparedStatement::getNumberOfColumns() const{

        return this->column_count;

    }


    string PreparedStatement::describeColumnType( int column_type ) const{

        switch( column_type ){
            case SQLITE_INTEGER:
                return "SQLITE_INTEGER";
            case SQLITE_FLOAT:
                return "SQLITE_FLOAT";
            case SQLITE_TEXT:
                return "SQLITE_TEXT";
            case SQLITE_BLOB:
                return "SQLITE_BLOB";
            case SQLITE_NULL:
                return "SQLITE_NULL";
            default:
                return "Unknown Type";
        };

    }



}
