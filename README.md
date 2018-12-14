# logport

Logport watches a log file for changes and sends batches of lines to kafka.

No partial line is sent to kafka. But, multiple lines (newline separated) will be sent in a single kafka message.

Logport will exit after a successful logrotate event. Logport should be used with a start-stop daemon to restart it.


## Dependencies
- rdkafka ( https://syslogng-kafka.readthedocs.io/en/latest/installation_librdkafka.html or see OEL511.compile)

## Running
```
wget -O logport https://github.com/homer6/logport/blob/master/build/logport?raw=true
chmod ugo+x logport
ldd logport
#./build/logport <bootstrap-brokers-list> <topic> <file-to-watch>
./build/logport 127.0.0.1 hello sample.log
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


# Running Example

```
#./build/logport <bootstrap-brokers-list> <topic> <file-to-watch>
./build/logport 127.0.0.1 hello sample.log
valgrind --leak-check=yes ./build/logport 127.0.0.1 hello sample.log
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