# logport

Logport watches a log file for changes and lines to kafka (one line per message).

## Requirements
- ubuntu 18 or OEL 5.11 ( please open an issue if you'd like support for your platform )
- 64 bit linux
- libc 2.5+
- linux kernel 2.6.9+

## Dependencies
- rdkafka ( build included, but you can also install or build your own: https://syslogng-kafka.readthedocs.io/en/latest/installation_librdkafka.html or see OEL511.compile)

## Installing, running as a service, and adding files to watch
```
# download the installer/service/agent (all three in 1 binary)
wget -O logport https://github.com/homer6/logport/blob/master/build/librdkafka.so.1?raw=true
wget -O logport https://github.com/homer6/logport/blob/master/build/logport?raw=true
chmod ugo+x logport

# Install, start, and enable the service. This also sets the default topic and brokers list.
sudo ./logport install my_syslog_topic kafka1:9092,kafka2:9092,kafka3:9092

# Delete the downloaded files (optional)
rm librdkafka.so.1
rm logport

# Add some files to be watched (this uses the default topic and brokers list above, 
# but you can specify non-defaults with the --topic and --brokers flags).
# You can specify many files (or patterns to add many files).
logport watch /var/log/*.log /var/log/syslog
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