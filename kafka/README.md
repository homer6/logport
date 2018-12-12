

# Install kafka standalone:

```
apt install ansible
ansible-galaxy install sansible.kafka
ansible-galaxy install sansible.zookeeper
ansible-playbook kafka/app.yml

```

See https://github.com/sansible/kafka for non-local installations.


# Watch log entries

```
apt install kafkacat
kafkacat -C -b 127.0.0.1 -t hello
```
