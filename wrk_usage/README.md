`wrk` 是目前最流行、最好用的 HTTP 基准压测工具之一。它基于 LuaJIT，利用多线程和非阻塞 IO，能够模拟出极高的并发压力，非常适合用来测试你刚才写的 Epoll 服务器。

以下是 `wrk` 的**安装**、**核心参数**、**常用命令**以及**结果解读**指南。

---

### 1. 安装 wrk

#### macOS (推荐用 Homebrew)
```bash
brew install wrk
```

#### Linux (Ubuntu/Debian)
大多数源里没有最新版，建议源码编译（很简单）：
```bash
sudo apt-get install build-essential libssl-dev git -y
git clone https://github.com/wg/wrk.git
cd wrk
make
sudo cp wrk /usr/local/bin/
```

#### Windows
Windows 原生不支持 `wrk`。
- **方案 A (推荐)**: 使用 WSL2 (Windows Subsystem for Linux)，在 WSL 里按 Linux 方法安装。
- **方案 B**: 下载预编译的 `.exe` (GitHub 上有第三方编译版，但不如 WSL 稳定)。

---

### 2. 核心参数详解

`wrk` 的基本语法：
```bash
wrk [选项] <URL>
```

| 参数 | 简写 | 说明 | 典型值 |
| :--- | :--- | :--- | :--- |
| `--threads` | `-t` | **线程数**。通常设置为 CPU 核心数。 | `-t4` (4核) |
| `--connections` | `-c` | **总并发连接数**。这是压测的关键指标。 | `-c100`, `-c1000` |
| `--duration` | `-d` | **压测持续时间**。时间太短数据不准。 | `-d10s`, `-d30s` |
| `--latency` | `-L` | **打印延迟统计**。默认不显示，**必加**。 | `-L` |
| `--script` | `-s` | **Lua 脚本**。用于复杂场景（如 POST、动态Header）。 | `-s post.lua` |
| `--timeout` | | 超时时间。防止慢请求卡死测试。 | `--timeout 2s` |

---

### 3. 实战：测试你的 HTTP 服务器

假设你的服务器运行在 `http://localhost:8080`。

#### 场景 A：基础冒烟测试 (低并发)
先看看服务器能不能正常响应。
```bash
wrk -t2 -c10 -d5s http://localhost:8080/
```
- **含义**: 2个线程，10个并发连接，跑5秒。

#### 场景 B：标准性能压测 (推荐)
模拟中等负载，查看 QPS 和延迟分布。
```bash
wrk -t4 -c100 -d30s --latency http://localhost:8080/
```
- **含义**: 4个线程，100个并发连接，跑30秒，**显示延迟详情**。
- **注意**: `-t` 一般设为机器 CPU 核心数。`-c` 可以逐步增加 (100, 500, 1000...) 来寻找服务器的瓶颈。

#### 场景 C：高并发压力测试
尝试把服务器打满。
```bash
wrk -t8 -c1000 -d60s --latency http://localhost:8080/
```
- **含义**: 8个线程，1000个并发连接，跑1分钟。
- **观察**: 此时你的服务器 CPU 应该飙高了，观察 QPS 是否下降，延迟是否剧增。

#### 场景 D：测试 POST 请求 (带 Body)
如果你的服务器需要处理 POST 数据，需要写一个简单的 Lua 脚本。

1. 创建 `post.lua`:
```lua
wrk.method = "POST"
wrk.body   = "name=test&value=123"
wrk.headers["Content-Type"] = "application/x-www-form-urlencoded"
```

2. 运行:
```bash
wrk -t4 -c100 -d30s -s post.lua http://localhost:8080/api/submit
```

---

### 4. 如何看懂输出结果？

运行结束后，你会看到类似这样的输出：

```text
Running 30s test @ http://localhost:8080/
  4 threads and 100 connections
  
  # 1. 吞吐量部分
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.20ms    0.50ms   15.00ms   90.00%   <-- 延迟分布
    Req/Sec     25.00k     1.20k    28.00k   85.00%   <-- 每秒请求数 (QPS)

  # 2. 总统计
  750123 requests in 30.00s, 50.00MB read
  
  # 3. 错误统计 (非常重要！)
  Socket errors: connect 0, read 0, write 0, timeout 5  <-- 有5个超时
  Non-2xx or 3xx responses: 0                          <-- 没有HTTP错误

  # 4. 延迟详情 (因为加了 --latency)
  Latency Distribution
     50%    1.10ms
     75%    1.30ms
     90%    1.80ms
     99%    5.00ms   <-- P99 延迟，代表最慢的那 1% 的请求耗时
```

#### 🔑 关键指标解读：

1.  **Req/Sec (QPS)**:
    *   这是最核心的性能指标。数字越大越好。
    *   如果你发现增加 `-c` (并发数) 后，这个数字不再增长甚至下降，说明服务器达到瓶颈了。

2.  **Latency (延迟)**:
    *   **Avg**: 平均延迟。容易被极端值拉偏，参考意义一般。
    *   **Max**: 最大延迟。偶尔出现的长尾。
    *   **90% / 99% (P99)**: **最重要！** 代表 99% 的请求都在这个时间内完成。如果 P99 很高（比如几秒），说明系统不稳定，有卡顿。

3.  **Socket errors**:
    *   **connect 0**: 连接失败数。如果不为 0，说明服务器拒绝连接或端口满了。
    *   **read/write 0**: 读写错误。如果不为 0，可能是你的服务器代码里有 Bug（比如提前 close 了连接，或者 buffer 溢出）。
    *   **timeout**: 超时数。如果不为 0，说明服务器处理太慢，超过了 `wrk` 的等待时间。

---

### 5. 针对你服务器的调试建议

既然你在开发阶段，建议按以下步骤“折磨”你的服务器：

1.  **第一步：验证 Keep-Alive**
    *   先用 `-c10` 跑一下。
    *   然后修改代码，**注释掉 `close(fd)`** (实现长连接)。
    *   再用 `-c10` 跑一下。
    *   **预期**: QPS 应该会有显著提升（可能翻倍），因为省去了握手开销。

2.  **第二步：寻找崩溃点**
    *   逐渐增加 `-c`：100 -> 500 -> 1000 -> 5000。
    *   观察 `Socket errors`。一旦开始出现 `read` 或 `write` 错误，说明你的缓冲区逻辑或非阻塞处理有问题（比如没处理 `EAGAIN`）。
    *   观察服务器进程是否 Segfault (段错误)。如果有，说明有内存越界或悬空指针（比如之前讨论的 `ctx` 释放问题）。

3.  **第三步：观察 P99 延迟**
    *   在高并发下，如果 Avg 延迟很低，但 P99 很高，说明你的线程池可能有排队现象，或者发生了锁竞争。

**现在，快去终端试试 `wrk -t4 -c100 -d10s --latency http://localhost:8080/` 吧！** 看看你的服务器能跑多少 QPS？