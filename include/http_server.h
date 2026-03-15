#pragma once

#include "log.h"
#include "common.h"
#include "thread_pool.h"
#include "connect_context.h"

#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


// ================= 设置非阻塞IO =================
void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class HttpServer
{
private:
    ThreadPool m_thread_pool;
    int m_epoll_fd;
    int m_listen_fd;

private:
    void handle_client_task(ConnectionContext *ctx) {
        // 如果已标记关闭，直接释放引用退出
        if (ctx->closed.load())
        {
            ctx->release();
            return;
        }

        bool should_close = false;
        bool need_readd = false;

        // 读取数据
        ssize_t n = read(ctx->fd, ctx->buffer + ctx->bytes_read, BUFFER_SIZE - ctx->bytes_read - 1);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 数据未读完，需要继续监听
                need_readd = true;
            }
            else
            {
                // 真实错误
                if (errno == EBADF)
                {
                    ctx->closed.store(true);
                    ctx->release();
                    return;
                }
                // std::cerr << "Read error: " << strerror(errno) << " (fd=" << ctx->fd << ")" << std::endl;
                should_close = true;
            }
        }
        else if (n == 0)
        {
            // 对端关闭
            should_close = true;
        }
        else
        {
            ctx->bytes_read += n;
            ctx->buffer[ctx->bytes_read] = '\0';

            if (ctx->request.parse(ctx->buffer))
            {
                std::string resp = build_response(ctx->request);
                ssize_t sent = write(ctx->fd, resp.c_str(), resp.size());
                if (sent < 0 && errno != EAGAIN)
                {
                    // std::cerr << "Write error: " << strerror(errno) << std::endl;
                }
                // HTTP/1.0 或 Connection: close，处理完即关闭
                should_close = true;
            }
            else
            {
                // 请求不完整
                if (ctx->bytes_read >= BUFFER_SIZE - 1)
                {
                    // 缓冲区满，强制关闭
                    should_close = true;
                }
                else
                {
                    // 继续接收
                    need_readd = true;
                }
            }
        }

        if (should_close)
        {
            // 原子标记，防止重复关闭逻辑
            if (ctx->closed.exchange(true))
            {
                ctx->release();
                return;
            }

            // 从 epoll 移除 (即使 ONESHOT 也可能需要显式 DEL 以防万一，或者清理状态)
            // 注意：如果是 ONESHOT 触发的事件，它已经自动禁用了，但显式 DEL 是个好习惯
            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, ctx->fd, nullptr);
            close(ctx->fd);

            // 任务结束，释放引用
            ctx->release();
        }
        else if (need_readd)
        {
            // 再次检查是否被关闭（可能在 read/write 期间被其他逻辑标记？虽然 ONESHOT 保证了单线程，但防御性编程）
            if (ctx->closed.load())
            {
                ctx->release();
                return;
            }

            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // 👈 关键：必须带上 ONESHOT
            ev.data.ptr = static_cast<void *>(ctx);

            // 使用 EPOLL_CTL_MOD 重新启用监听
            // 因为 fd 本来就在 epoll 里（只是被 ONESHOT 禁用了），所以用 MOD
            // 如果用 ADD 会报 File exists
            if (epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev) == -1)
            {
                // 如果 MOD 失败（比如 fd 已经关了），则视为关闭
                // std::cerr << "Re-add (MOD) failed: " << strerror(errno) << " (fd=" << ctx->fd << ")" << std::endl;
                ctx->closed.store(true);
                close(ctx->fd); // 尝试关闭
                // 无论成功失败，都要释放当前任务的引用
            }

            // 任务结束，释放引用
            // 注意：这里释放的是“当前任务”持有的引用。
            // 如果 MOD 成功，epoll 重新持有了“逻辑引用”（实际上引用计数不需要变，因为我们在主线程取出时会 add_ref，这里 release 平衡掉）
            // 逻辑闭环：
            // 1. 主线程取出: add_ref (count++)
            // 2. 主线程 DEL/MOD 前：此时 count 包含任务持有。
            // 3. 任务结束: release (count--)。
            // 4. 如果 MOD 成功：fd 回到 epoll 等待状态。下次触发时，主线程再次 add_ref。
            // 这样引用计数始终保持平衡：在 epoll 队列中时 count=1，在处理时 count=2 (短暂)，处理完变回 1。
            ctx->release();
        }
        else
        {
            // 其他情况，释放引用
            ctx->release();
        }
    }

    void event_loop() {
        // ================= 创建epoll =================
        m_epoll_fd = epoll_create1(0);
        struct epoll_event ev{};
        ev.events = EPOLLIN; // Listen fd 不需要 ONESHOT
        ev.data.fd = m_listen_fd;
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_fd, &ev);

        struct epoll_event events[MAX_EVENTS];

        while (true)
        {
            int n_fds = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);
            if (n_fds < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("epoll_wait");
                break;
            }
    
            for (int i = 0; i < n_fds; ++i)
            {
                int fd = events[i].data.fd;
    
                if (fd == m_listen_fd)
                {
                    struct sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int conn_fd = accept(m_listen_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (conn_fd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        perror("accept");
                        continue;
                    }
                    set_nonblocking(conn_fd);
    
                    ConnectionContext *ctx = new ConnectionContext(conn_fd);
                    // ref_count = 1
    
                    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // 👈 关键：新连接必须注册为 ONESHOT + ET
                    ev.data.ptr = static_cast<void *>(ctx);
    
                    if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1)
                    {
                        perror("epoll_ctl add");
                        delete ctx;
                        close(conn_fd);
                        continue;
                    }
                    // 此时 ref_count = 1 (epoll 持有)
                }
                else
                {
                    ConnectionContext *ctx = static_cast<ConnectionContext *>(events[i].data.ptr);
                    if (!ctx)
                        continue;
    
                    // 👉 关键步骤：
                    // 1. 增加引用，表示“任务”持有了对象
                    ctx->add_ref();
    
                    // 2. 此时对象由 (epoll逻辑持有 + 任务持有) 共同保护。
                    // 由于是 ONESHOT，fd 已经被内核自动禁用，不会再次触发。
                    // 我们不需要显式 DEL，因为 ONESHOT 已经做了。
                    // 但为了逻辑清晰，我们可以认为 epoll 的“激活状态”结束了。
                    // 引用计数模型：
                    //   - 在 epoll 队列等待时：ref = 1
                    //   - 触发后，主线程 add_ref -> ref = 2
                    //   - 提交任务。
                    //   - 任务结束时 release -> ref = 1 (回到 epoll 等待状态，或者如果关闭则 ref=0)
                    //   - 如果需要 re-add (MOD)，内核重新激活，ref 保持 1。
                    
                    m_thread_pool.enqueue(std::bind(&HttpServer::handle_client_task, this, ctx));
                }
            }
        }
    }

public:
    HttpServer(size_t thread_pool_size=THREAD_POOL_SIZE);
    virtual ~HttpServer();

    // 开启监听并启用服务
    void Serve(int port=LISTEN_PORT);
};

HttpServer::HttpServer(size_t thread_pool_size) : m_thread_pool{thread_pool_size}
{
}

HttpServer::~HttpServer()
{
}


void HttpServer::Serve(int port) {
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    // ================= 设置地址复用 =================
    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ================= bind =================
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(m_listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    // ================= listen =================
    if (listen(m_listen_fd, 128) < 0)
    {
        perror("listen");
        exit(1);
    }
    set_nonblocking(m_listen_fd);

    std::cout << "Server running (EPOLLONESHOT + RefCount)" << std::endl;
    std::cout << "Server running on: http://localhost:" + std::to_string(port) << std::endl;

    // 运行事件循环
    event_loop(); 

    // 循环退出关闭fd
    close(m_listen_fd);
    close(m_epoll_fd);
}