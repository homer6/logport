#ifndef LOGPORT_LOGPORT_H
#define LOGPORT_LOGPORT_H

#include <string>
using std::string;

#include <vector>
using std::vector;


class LogPort{

    public:
        LogPort( const string &kafka_connection_string, const vector<string> &watched_files );
        void startWatching();

    protected:
        string kafka_connection_string;
        vector<string> watched_files;

};


#endif //LOGPORT_LOGPORT_H
