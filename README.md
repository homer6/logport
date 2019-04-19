# logport

Logport watches a log file for changes and sends batches of lines to kafka.


## Dependencies
- ubuntu 18 ( please open an issue if you'd like support for your platform )
- rdkafka ( https://syslogng-kafka.readthedocs.io/en/latest/installation_librdkafka.html or see OEL511.compile)
- 64 bit
- libc 2.5+
- linux kernel 2.6.9+

## Running
```
# download the installer/service/agent (all three in 1 binary)
wget -O logport https://github.com/homer6/logport/blob/master/build/logport?raw=true
chmod ugo+x logport

# ensure logport is properly linking against librdkafka
ldd logport

# Install and enable the service. And, add the first watch to the local logport service.
# Subsequent `logport watch` commands do not require the broker or topic. The first
# topic and broker are set to the default.
#
# eg.
#    second run:   logport watch /var/log/kern.log
#    third run:    logport watch /var/log/my_app_log.txt my_app_log
#
# Both the second and third run above will be configured with the first broker.
# The third run will also use `my_app_log` as the topic.
# `sudo ./logport` is also unnessary, too, because the service is already installed 
# (no root required) and the logport binary is now in the PATH.
#
sudo ./logport watch /var/log/syslog my_syslog_topic kafka1:9092,kafka2:9092,kafka3:9092
```


## logport --help
```
usage: logport [--version] [--help] <command> [<args>]

These are common logport commands used in various situations:

add system service
   install    Installs logport as a system service (and enables it)
   uninstall  Removes logport service and configuration
   enable     Enables the service to start on bootup
   disable    Disables the service from starting on bootup

systemd commands
   start      Starts the service
   stop       Stops the service
   restart    Restarts the service gracefully
   status     Prints the running status of logport
   reload     Explicitly reloads the configuration file

manage watches
   watch      Add a watch (will also implicitly install logport)
   unwatch    Remove a watch
   watches    List all watches

manage settings
   set        Set a setting's value
   unset      Clear a setting's value

Please see: https://github.com/homer6/logport to report issues
or view documentation.
```



## Building
```
cmake .
make
```


See kafka/README.md if you'd like to install a local copy of kafka (ubuntu only)



# Logging Example
```
while true; do echo "sample log entry at `date`" >> sample.log; sleep 1; done
```


# Watching Example

```
kafkacat -C -b 127.0.0.1 -t hello -f 'Topic %t [%p] at offset %o: key %k: %s\n'
```



# Logrotate Example

## Logrotate Configuration Example

```
root@node-1w7jr9qh6y35wr9tbc3ipkal4:/etc/logrotate.d# cat sample
/home/user/logport/sample.log
{
        rotate 4
        daily
        delaycompress
        missingok
        notifempty
        compress
}

```

## Logrotate Testing Example

```
logrotate -v -f /etc/logrotate.d/sample
```


# TODOs

 - refactor the copy out of the kafka_produce method, if possible
 - consider reducing the polling frequency
 - consider making batching configurable