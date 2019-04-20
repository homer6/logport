#include "Watch.h"
#include "PreparedStatement.h"

namespace logport{


    Watch::Watch()
        :id(0), file_offset(0)
    {   

    }

    Watch::Watch( const PreparedStatement& statement )
        :id(0), file_offset(0)
    {   

        this->id = statement.getInt64( 0 );
        this->watched_filepath = statement.getText( 1 );
        this->file_offset = statement.getInt64( 2 );
        this->brokers = statement.getText( 3 );
        this->topic = statement.getText( 4 );
    }

    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, int64_t file_offset )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic), id(0), file_offset(file_offset)
    {

    }


    void Watch::bind( PreparedStatement& statement, bool skip_id ) const{

        int current_offset = 1;

        if( !skip_id ){
            statement.bindInt64( current_offset++, this->id );
        }
        
        statement.bindText( current_offset++, this->watched_filepath );
        statement.bindInt64( current_offset++, this->file_offset );
        statement.bindText( current_offset++, this->brokers );
        statement.bindText( current_offset++, this->topic );

    }


}
