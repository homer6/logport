FROM ubuntu:latest

RUN mkdir -p /usr/local/lib/logport/install
ADD build/logport /usr/local/lib/logport/install/logport
ADD build/librdkafka.so.1 /usr/local/lib/logport/install/librdkafka.so.1
WORKDIR /usr/local/lib/logport/install
RUN /usr/local/lib/logport/install/logport install

ENV LOGPORT_BROKERS 192.168.1.91
ENV LOGPORT_TOPIC my_logs
# ENV LOGPORT_PRODUCT_CODE prd4096
# ENV LOGPORT_LOG_TYPE system
ENV LOGPORT_HOSTNAME my.sample.hostname

ENTRYPOINT [ "logport", "adopt" ]
CMD [ "/usr/local/bin/logport", "hello" ]