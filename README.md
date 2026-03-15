## 锁


对于多线程下的公共资源访问必须互斥，一定要加锁



### 无内存池结果
```
Running 10s test @ http://localhost:8081/
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.29ms  330.00us   3.51ms   85.28%
    Req/Sec    10.53k   710.13    15.09k    93.00%
  Latency Distribution
     50%    2.34ms
     75%    2.47ms
     90%    2.58ms
     99%    2.79ms
  419398 requests in 10.00s, 146.39MB read
Requests/sec:  41937.49
Transfer/sec:     14.64MB
```

### 添加内存池结果
```
Running 10s test @ http://localhost:8081/
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.68ms    2.05ms  51.19ms   99.53%
    Req/Sec    13.61k     2.20k   19.90k    78.50%
  Latency Distribution
     50%    1.78ms
     75%    2.00ms
     90%    2.17ms
     99%    2.52ms
  541747 requests in 10.00s, 189.09MB read
Requests/sec:  54169.45
Transfer/sec:     18.91MB
```
