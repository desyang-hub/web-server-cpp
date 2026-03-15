#include "http_request.h"

std::string build_response(const HttpRequest &req)
{
    std::string body;
    std::string content_type = "text/html";
    int status_code = 200;
    std::string status_msg = "OK";

    // 简易路由逻辑
    if (req.path == "/" || req.path == "/index.html")
    {
        body = R"(
<!DOCTYPE html>
<html>
<head><meta charset='UTF-8'><title>Native C++ Server</title></head>
<body>
    <h1>你好！这是原生 C++ Epoll 服务器</h1>
    <p>路径: )" +
               req.path + R"(</p>
    <p>方法: )" +
               req.method + R"(</p>
    <hr>
    <a href="/api/data">点击测试 API</a>
</body>
</html>)";
    }
    else if (req.path == "/api/data")
    {
        content_type = "application/json";
        body = "{\"status\":\"success\", \"message\":\"Hello from Epoll Server!\", \"path\":\"" + req.path + "\"}";
    }
    else
    {
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