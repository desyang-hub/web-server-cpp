#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <chrono>

template<class T>
class BlockedQueue
{
private:
    std::queue<T> m_que;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool stop{false};

public:
    BlockedQueue() {}
    virtual ~BlockedQueue() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stop = true;
        }
        m_condition.notify_all();
    }

public:
    template<class E>
    void push(E&& e) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_que.emplace(std::forword<E>(e));
        }
        m_condition.notify_one();
    }

    bool pop(T& ele) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_condition.wait(lock, [this]() {
                return stop || !m_que.empty();
            });

            if (stop && m_que.empty()) {
                return false;
            }

            ele = std::move(m_que.front());
            m_que.pop();

        }
        return true;
    }

    // 超时退出pop
    bool pop(T& ele, size_t ms_timeout) {
        // 开始计时
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms_timeout);

        std::unique_lock<std::mutex> lock(m_mutex);

        bool ret = m_condition.wait_until(lock, deadline [this, start_time](){
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
            return !m_que.empty() || stop;
        });
        
        if (!ret) {
            return false;
        }

        if (m_que.empty()) {
            return false;
        }

        ele = std::move(m_que.front());
        m_que.pop();

        return true;
    }
};