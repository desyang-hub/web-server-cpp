#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <sstream>

// ================= 配置常量 =================
const int MAX_EVENTS = 1024;
const int LISTEN_PORT = 8080;
const int THREAD_POOL_SIZE = 4;
const int BUFFER_SIZE = 4096;

// ================= 简易线程池 =================
class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// ================= HTTP 请求结构 =================
struct HttpRequest {
    std::string method;
    std::string path;
    std::string protocol;
    std::string body;
    bool is_complete = false;

    // 简单解析：只处理 GET 和简单的 HEAD/Body 分隔
    bool parse(const std::string& raw_data) {
        if (raw_data.find("\r\n\r\n") == std::string::npos) return false; // 头部没读完

        std::istringstream stream(raw_data);
        std::string line;
        
        // 1. 解析第一行: GET /index.html HTTP/1.1
        if (!std::getline(stream, line)) return false;
        // 去掉 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        std::istringstream first_line(line);
        first_line >> method >> path >> protocol;

        // 这里可以扩展解析 Headers，本 Demo 略过，直接认为头部结束
        is_complete = true;
        return true;
    }
};

// ================= HTTP 响应构建 =================
std::string build_response(const HttpRequest& req) {
    std::string body;
    std::string content_type = "text/html";
    int status_code = 200;
    std::string status_msg = "OK";

    // 简易路由逻辑
    if (req.path == "/" || req.path == "/index.html") {
        body = R"(
<!DOCTYPE html>
<html>
<head><meta charset='UTF-8'><title>Native C++ Server</title></head>
<body>
    <h1>你好！这是原生 C++ Epoll 服务器</h1>
    <p>路径: )" + req.path + R"(</p>
    <p>方法: )" + req.method + R"(</p>
    <hr>
    <a href="/api/data">点击测试 API</a>
</body>
</html>)";
    } else if (req.path == "/api/data") {
        content_type = "application/json";
        body = "{\"status\":\"success\", \"message\":\"Hello from Epoll Server!\", \"path\":\"" + req.path + "\"}";
    } else {
        status_code = 404;
        status_msg = "Not Found";
        body = "<h1>404 Not Found</h1><p>The requested path was not found.</p>";
    }

    // 构建 HTTP 响应头
    std::string response = 
        "HTTP/1.1 " + std::to_string(status_code) + " " + status_msg + "\r\n" +
        "Content-Type: " + content_type + "; charset=utf-8\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Connection: close\r\n" + // 简单起见，处理完即关闭连接
        "\r\n" +
        body;

    return response;
}

// ================= 连接上下文 (用于 Epoll 状态管理) =================
struct ConnectionContext {
    int fd;
    char buffer[BUFFER_SIZE];
    int bytes_read = 0;
    HttpRequest request;
    
    ConnectionContext(int _fd) : fd(_fd), bytes_read(0) {
        memset(buffer, 0, BUFFER_SIZE);
    }
};

// ================= 全局变量 =================
int epoll_fd;
ThreadPool pool(THREAD_POOL_SIZE);

// 设置非阻塞
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 处理客户端数据的任务函数
void handle_client_task(ConnectionContext* ctx) {

    bool should_close{false};
    bool should_read{false};

    // 1. 继续读取数据 (因为是非阻塞，可能一次读不完)
    ssize_t n = read(ctx->fd, ctx->buffer + ctx->bytes_read, BUFFER_SIZE - ctx->bytes_read - 1);
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 暂时没数据，保留连接，等待下次 epoll 触发
            should_read = true; 
        } else {
            // 已经关闭了
            if (errno == EBADF) {
                delete ctx;
                return;
            }
            // 真实错误
            std::cerr << "Read error: " << strerror(errno) << std::endl;
            should_close = true;
        }
    } else if (n == 0) {
        // 客户端关闭连接
        should_close = true;
    }
    else { // 用户读数据

        ctx->bytes_read += n;
        ctx->buffer[ctx->bytes_read] = '\0';

        // 2. 尝试解析 HTTP
        if (ctx->request.parse(ctx->buffer)) {
            // 解析成功，构建响应
            std::string response = build_response(ctx->request);
            
            // 3. 发送响应 (简化版：一次性 send，实际生产需处理 EAGAIN 循环发送)
            ssize_t sent = write(ctx->fd, response.c_str(), response.size());
            if (sent < 0) {
                std::cerr << "Write error" << std::endl;
            }
            
            // 4. 关闭连接 (因为 Header 里写了 Connection: close)
            // 如果要做 Keep-Alive，这里需要重置 ctx 状态并重新加入 epoll 监听读事件
            should_close = true;
        }
        else { // 还没读取完成
            if (ctx->bytes_read >= BUFFER_SIZE - 1) {
                // 超出缓冲区，直接关闭
                should_close = true;
            }
            else {
                should_read = true;
            }
        }

    }

    if (should_close) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, nullptr);
        close(ctx->fd);
        delete ctx;
    }
    else if (should_read) {
        struct epoll_event ev{};
        ev.data.ptr = static_cast<ConnectionContext*>(ctx);
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev) == -1) {
            // 已经存在了
            delete ctx;
        }
    }
    else {
        close(ctx->fd);
        delete ctx;
    }
}

int main() {
    // 1. 创建 Socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    // 地址复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); return 1;
    }

    // 监听
    if (listen(listen_fd, 128) < 0) { perror("listen"); return 1; }
    set_nonblocking(listen_fd);

    std::cout << "Server running on port " << LISTEN_PORT << " (Epoll + Thread Pool)" << std::endl;

    // 2. 创建 Epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create"); return 1; }

    // 注册监听 socket
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];

    // 3. 事件循环 (Reactor)
    while (true) {
        int n_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n_fds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n_fds; ++i) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // 接受新连接
                struct sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (conn_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    perror("accept");
                    continue;
                }
                set_nonblocking(conn_fd);
                
                // 注册新连接到 epoll
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT; 
                // 注意：这里没有用 EPOLLET (边缘触发)，使用的是 LT (水平触发)
                // LT 模式下，只要缓冲区有数据，就会一直通知，适合新手处理半包
                ev.data.fd = conn_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
                
                // std::cout << "New connection: " << conn_fd << std::endl;
            } else {
                
                // 创建上下文对象 (堆分配，由任务完成后删除)
                ConnectionContext* ctx = new ConnectionContext(fd);
                
                // 提交给线程池
                pool.enqueue([ctx]() {
                    handle_client_task(ctx);
                });
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}