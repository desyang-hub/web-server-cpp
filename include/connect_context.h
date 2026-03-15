#pragma once

#include <atomic>
#include <memory>
#include <string.h>

#include "common.h"
#include "http_request.h"
#include "alloc/allocator.h"

std::string build_response(const HttpRequest &req);

struct ConnectionContext {
    // 使用内存池说明
    DECLARE_POOL_ALLOC();

public:
    int fd;
    char buffer[BUFFER_SIZE];
    int bytes_read = 0;
    HttpRequest request;
    std::atomic<bool> closed{false};
    // 引用计数：初始为 1 (代表被 epoll 或创建者持有)
    std::atomic<int> ref_count{1}; 

    ConnectionContext(int _fd) : fd(_fd), bytes_read(0) {
        memset(buffer, 0, BUFFER_SIZE);
    }

    void add_ref() {
        ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release() {
        if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
};