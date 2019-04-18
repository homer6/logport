#include "Watch.h"

namespace logport{


    Watch::Watch(){   

    }

    Watch::Watch( const string& watched_filepath, const string& undelivered_log_filepath, const string& brokers, const string& topic )
        :watched_filepath(watched_filepath), undelivered_log_filepath(undelivered_log_filepath), brokers(brokers), topic(topic)
    {

    }


}