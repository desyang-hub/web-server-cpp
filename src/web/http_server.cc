#include "web/http_server_impl.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <functional>


namespace web
{

// 使用工厂方法创建实例
HttpServer* CreateHttpServer() {
    return new HttpServerImpl();
}

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

} // namespace web
