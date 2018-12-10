#include "LogPort.h"

int main(){

    LogPort log_port( "127.0.0.1:9092", {
        "/var/log/syslog"
    });

    log_port.startWatching();

    return 0;

}