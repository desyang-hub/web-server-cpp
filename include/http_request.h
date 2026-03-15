#pragma once

#include <iostream>
#include <string>
#include <sstream>

struct HttpRequest
{
    std::string method, path, protocol;
    bool is_complete = false;
    bool parse(const std::string &raw)
    {
        if (raw.find("\r\n\r\n") == std::string::npos)
            return false;

        std::istringstream s(raw);
        std::string line;

        if (!std::getline(s, line))
            return false;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::istringstream f(line);
        f >> method >> path >> protocol;
        is_complete = true;
        return true;
    }
};


// ================= HTTP 响应构建 =================
