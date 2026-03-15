// #define CPPHTTPLIB_OPENSSL_SUPPORT // 如果需要 HTTPS 则开启，HTTP 可不需要这行，但加上无妨
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sstream>

// 辅助函数：读取文件内容
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(void) {
    using namespace httplib;

    Server svr;

    // 1. 处理根路径请求，显示 index.html
    svr.Get("/", [](const Request& req, Response& res) {
        std::string html = read_file("./index.html");
        
        if (html.empty()) {
            res.status = 404;
            res.set_content("404 Not Found: index.html not found", "text/plain");
            return;
        }

        res.set_content(html, "text/html");
    });

    // 2. 处理一个简单的 GET 请求 (API 示例)
    // 访问 http://localhost:8080/api/get-data?name=Tom
    svr.Get("/api/get-data", [](const Request& req, Response& res) {
        // 获取 URL 参数，例如 ?name=Tom
        std::string name = req.has_param("name") ? req.get_param_value("name") : "Guest";
        
        // 构造简单的 JSON 响应 (实际项目建议用 nlohmann/json 库)
        std::string json_response = "{\"status\":\"success\", \"message\":\"Hello " + name + "\", \"code\":200}";

        res.set_content(json_response, "application/json");
    });

    // 启动服务器
    std::cout << "Server starting on http://localhost:8080 ..." << std::endl;
    
    // listen(端口号)
    auto ret = svr.listen("0.0.0.0", 8080);

    if (ret) {
        std::cout << "Server stopped." << std::endl;
    } else {
        std::cerr << "Failed to start server (port might be in use)." << std::endl;
    }

    return 0;
}