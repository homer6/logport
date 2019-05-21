


## Snowflake REST API

https://docs.snowflake.net/manuals/user-guide/data-load-snowpipe-rest.html


## ES-backed Jaeger

https://www.jaegertracing.io/docs/1.12/deployment/#elasticsearch


## Elasticsearch Operator

https://github.com/upmc-enterprises/elasticsearch-operator

https://www.elastic.co/guide/en/cloud-on-k8s/current/index.html

https://www.datadoghq.com/blog/elasticsearch-performance-scaling-problems/

## Druid

http://druid.io/

https://github.com/helm/charts/tree/master/incubator/druid


## Grafana

https://grafana.com/plugins/abhisant-druid-datasource/installation

https://github.com/helm/charts/tree/master/stable/grafana


## Superset

https://superset.incubator.apache.org/


## Prometheus

https://prometheus.io/docs/introduction/first_steps/


## k8s CRDs

https://kubernetes.io/docs/tasks/access-kubernetes-api/custom-resources/custom-resource-definitions/



## Tracing Schema


https://opentracing.io/specification/

https://github.com/opentracing/opentracing-cpp

https://github.com/DataDog/dd-opentracing-cpp/blob/d5007f85385241cba4b0ac356f76f2e42f456925/src/span.h#L21

```
struct SpanData {

  std::string type;
  std::string service;
  std::string resource;
  std::string name;
  uint64_t trace_id;
  uint64_t span_id;
  uint64_t parent_id;
  int64_t start;
  int64_t duration;
  int32_t error;
  std::unordered_map<std::string, std::string> tags; 
  std::unordered_map<std::string, int> metrics;

  uint64_t traceId() const;
  uint64_t spanId() const;
  const std::string env() const;
  
}
```

https://github.com/DataDog/dd-opentracing-cpp/blob/3ab5fdad2a7ae4dd0baef50a328f93341fba3087/src/propagation.h#L49

```
class SpanContext : public ot::SpanContext {
  uint64_t id_;
  uint64_t trace_id_;
  std::string origin_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> baggage_;
};
```

```
const (
    SpanKindUnspecified = iota
    SpanKindServer
    SpanKindClient
)
```

https://godoc.org/go.opencensus.io/trace#SpanData

https://github.com/googleapis/googleapis/blob/master/google/devtools/cloudtrace/v1/trace.proto