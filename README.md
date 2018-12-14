# logport
File watching to kafka



cmake -Wno-dev .
make



wget https://raw.githubusercontent.com/homer6/logport/master/install.sh -O - | sh








//TODO:

refactor the copy out of the kafka_produce method, if possible


See kafka/README.md if you'd like to install a local copy of kafka (ubuntu only)



# Logging
```
while true; do echo "sample log entry at `date`" >> sample.log; sleep 1; done
```


# Running

```
#./build/logport <bootstrap-brokers-list> <topic> <file-to-watch>
./build/logport 127.0.0.1 hello sample.log
valgrind --leak-check=yes ./build/logport 127.0.0.1 hello sample.log
```


# Watching

```
kafkacat -C -b 127.0.0.1 -t hello -f 'Topic %t [%p] at offset %o: key %k: %s\n'
```



# Logrotate

## Configuration

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

## Testing

```
logrotate -v -f /etc/logrotate.d/sample
```