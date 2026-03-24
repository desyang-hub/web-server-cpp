#pragma once

#include <string>
#include <functional>

#include "web/http_request.h"

namespace web
{

class HttpServer {
public:
    virtual ~HttpServer() = default;
public:
    // 声明接口
    virtual bool Server(int port) = 0;

    // 设置连接回调函数
    virtual void SetConnectionCallBack(std::function<void(int)> callback) = 0;

    // 设置消息回调函数
    virtual void SetMessageCallBack(std::function<void(int, const HttpRequest&)> callback) = 0;
};

// 创建实例
HttpServer* CreateHttpServer();

/// @brief 设置非阻塞
/// @param fd 
void set_nonblocking(int fd);

} // namespace rpc
