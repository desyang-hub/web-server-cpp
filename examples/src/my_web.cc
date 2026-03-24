#include "web/http_server_impl.h"
#include <memory>

#include <iostream>

using namespace std;

class WebSite : public web::HttpServerImpl {
protected:
    void OnMessage(int client_fd, const web::HttpRequest& request) override {
        
        int status_code = 200;
        std::string status_msg = "OK";
        std::string content_type = "application/json";
        string body = "{\"path\": \"" + request.path + "\"}";

        // 构建 HTTP 响应头
        std::string response =
        "HTTP/1.1 " + std::to_string(status_code) + " " + status_msg + "\r\n" +
        "Content-Type: " + content_type + "; charset=utf-8\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Connection: close\r\n" + // 简单起见，处理完即关闭连接
        "\r\n" +
        body;

        if (write(client_fd, response.data(), response.size())) {
            std::cerr << "send error " << endl;
            return;
        }
    }

    void OnConnection(int client_fd) override {
        cout << "client " << client_fd << " 连接到服务器" << endl;
    }
};

std::unique_ptr<web::HttpServer> CreateHttpServer() {
    return std::make_unique<WebSite>();
}

int main() {

    std::unique_ptr<web::HttpServer> server = CreateHttpServer();

    server->Server(8080);

    cout << "Press any key quit...";
    cin.get();

    return 0;
}