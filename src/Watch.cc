#include "Watch.h"

namespace logport{


    Watch::Watch()
        :file_offset(0)
    {   

    }

    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic, int64_t file_offset )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic), file_offset(file_offset)
    {

    }


}
