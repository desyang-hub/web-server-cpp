#include "web/http_server.h"
#include <memory>

#include <iostream>

using namespace std;

int main() {
    
    auto server = std::unique_ptr<web::HttpServer>(web::CreateHttpServer());
    // HttpServer server(4);
    server->Server(8080);

    cout << "Press any key quit...";

    cin.get();

    return 0;
}