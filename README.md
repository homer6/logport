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
echo "sample log entry at `date`" >> sample.log
```


# Running

```
#./build/logport <bootstrap-brokers-list> <topic> <file-to-watch>
./build/logport 127.0.0.1 hello sample.log

```


# Watching

```
kafkacat -C -b 127.0.0.1 -t hello
```

