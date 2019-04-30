# logport

Logport watches log files and sends changes to kafka (one line per message).

## Features
- One dependency (librdkafka). Builds for both librdkafka and logport provided.
- Built with C++98 to support much older kernels and libc (but forward compatible too).
- Saves unsuccessful messages and replays them to ensure that no messages are lost.
- Saves successful offsets to prevent replaying sent log entries.
- Fast, efficient, and stable (~15k msg/s and 1.8MB of memory consumed).
- Automatically restarts watches if killed or on failure.
- Parent process monitors separate child processes and shuts them down if they exceed 256MB memory.
- Optionally collects system telemetry at configurable intervals.
- Integrated product codes for ease of adoption in enterprise settings.
- You can easily modify the code to filter or scrub data before it's sent to kafka.

## Requirements
- ubuntu 18 or OEL 5.11 ( please open an issue if you'd like support for your platform )
- 64 bit linux
- libc 2.5+
- linux kernel 2.6.9+
- /usr/local/logport/logport.db must not be on an NFS mount

## Dependencies
- rdkafka ( build included, but you can also install or build your own: https://syslogng-kafka.readthedocs.o/en/latest/installation_librdkafka.html or see OEL511.compile)

## Quickstart

```
wget -O librdkafka.so.1 https://github.com/homer6/logport/blob/master/build/librdkafka.so.1?raw=true
wget -O logport https://github.com/homer6/logport/blob/master/build/logport?raw=true
chmod ugo+x logport
sudo ./logport install
rm librdkafka.so.1
rm logport
logport set default.brokers 192.168.1.91
logport set default.topic my_logs
logport set default.product_code prd4096
logport set default.hostname my.sample.hostname
logport watch /usr/local/logport/*.log /var/log/syslog
logport watches
logport start
ps aux | grep logport
logport watches
```


## Installing, running as a service, and adding files to watch
```
# download the installer/service/agent (all three in 1 binary)
wget -O librdkafka.so.1 https://github.com/homer6/logport/blob/master/build/librdkafka.so.1?raw=true
wget -O logport https://github.com/homer6/logport/blob/master/build/logport?raw=true
chmod ugo+x logport

# Install the application
sudo ./logport install

# Delete the downloaded files (optional)
rm librdkafka.so.1
rm logport

# Add some files to watch (specify many here and/or with a pattern)
# Files may be relative, absolute, or symlinks (they'll all resolve to absolute paths).
logport watch --brokers kafka1:9092,kafka2:9092,kafka3:9092 --topic my_system_logs_topic --product-code prd4096 /var/log/syslog /var/log/*.log
logport watch --brokers localhost --topic new_topic --product-code prd1024 --hostname secret.host sample.log

# Check which files are being watched
logport watches

 watch_id | watched_filepath               | brokers                             | topic                | product_code | hostname           | file_offset_sent | pid
---------------------------------------------------------------------------------------------------------------------------------------------------------------------
        1 | /var/log/syslog                | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        2 | /var/log/alternatives.log      | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        3 | /var/log/apport.log            | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        4 | /var/log/auth.log              | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        5 | /var/log/bootstrap.log         | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        6 | /var/log/cloud-init.log        | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        7 | /var/log/cloud-init-output.log | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        8 | /var/log/dpkg.log              | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
        9 | /var/log/kern.log              | kafka1:9092,kafka2:9092,kafka3:9092 | my_system_logs_topic | prd123       | my.sample.hostname |                0 |  -1
       10 | /home/user/logport/sample.log  | localhost                           | new_topic            | prd1024      | secret.host        |                0 |  -1

# Before adding watches, you can specify brokers, topic, product code, or hostname 
# default settings (so you don't have a provide them to each watch). However, hostname
# itself defaults to the system's hostname. Adding it as a setting just overrides it.
logport set default.brokers 192.168.1.91
logport set default.topic my_logs
logport set default.product_code prd4096
logport set default.hostname my.sample.hostname

# If we want to ship logport's own logs, we can add them to be watched, too.
# By not providing the watch parameters here, we'll be using the default settings
# that we just established.
logport watch /usr/local/logport/*.log

# Enable the service on boot
logport enable

# Start the service
logport start

# Watch the logs with kafkacat
kafkacat -C -b 192.168.1.91 -o -10 -t my_logs
```


## logport format

Logport expects either unstructured single-line log lines or single-line JSON. 

If logport detects a left brace character `{` as the first character, it add the log
entry to the `log_obj` JSON key.

If logport does not detect a left brace, it will assume it to be single-line unstructured 
text and will escape the unstructured text to be embedded in the JSON produced by logport
(which is at the `log` JSON key).

### Unstructured Example

Unstructured Original Line: `my unstructured original log line abc123`

Unstructured Kafka Message: `{"shipped_at":1555955180.385583,"log":"my unstructured original log line abc123"}`

### Embedded JSON Example

JSON Original Line: `{"my":"custom","json":"object"}`

JSON Kafka Message: `{"shipped_at":1555955180.385583,"log_obj":{"my":"custom","json":"object"}}`

### Additional Fields

In both of the above cases, logport will include the additional fields of `host`, `source`, and `prd` (product code).

`{"shipped_at":1556352653.816769412,"host":"my.sample.hostname","source":"/usr/local/logport/logport.log","prd":"prd4096","log":"hello world"}`


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
   now        Watches a file temporarily (same options as watch)
   destory    Restore logport to factory settings. Clears all watches.

manage settings
   set        Set a setting's value
   unset      Clear a setting's value
   settings   List all settings
   destory    Restore logport to factory settings. Clears all watches.

collect telemetry
   inspect    Produce telemetry to telemetry log file

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

# Ephemeral Watch (logport now)

Running `logport now` will block. While running, it creates a temporary watch
to temporarily send log data to a topic. It takes the same options as `logport watch`.
But, unlike `logport watch`, watches are not retained after you exit. Also, it can
only watch one file (the first provided).
```
logport now --brokers 192.168.1.91 --topic my_logs sample.log
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

Warning: ensure that the parent directory, which contains the log file above, does not have writable permissions for 'others'.
Eg. `chmod o-w /home/user/logport`

## Logrotate Testing Example

```
logrotate -v -f /etc/logrotate.d/sample
```

# Roadmap

https://trello.com/b/o12As8ot/logport