#pragma once

#include <thread>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iostream>

namespace web
{
    
using Job = std::function<void()>;

class ThreadPool
{
private:
    // 标志对象即将销毁
    bool m_stop{false};
    // 队列信号量
    std::mutex m_mutex;
    // 条件信号量
    std::condition_variable m_condition;
    // 提交的任务
    std::queue<Job> m_jobs;
    // 工作线程
    std::vector<std::thread> m_workers;

private:
    void JobHandler() {
        Job job{nullptr};
        while (true)
        {
            // 每个任务处理的任务就是从m_jobs中取出任务，并执行
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(lock, [this](){
                    return !m_jobs.empty() || m_stop;
                });
                
                // ThreadPool 即将销毁，退出
                // if (m_stop) return;

                if (m_stop && m_jobs.empty()) {
                    // 安全退出
                    return;
                }
                
                // job = m_jobs.front(); // 禁止不必要的拷贝
                job = std::move(m_jobs.front()); 

                m_jobs.pop();                
            }

            // 执行任务
            // job();

            // 防止用户程序崩溃而导致整个线程退出
            try {
                job();
            } catch (const std::exception& e) {
                std::cerr << "Thread Pool Exception: " << e.what() << std::endl;
                // 可以选择记录日志，但不要终止线程
            } catch (...) {
                std::cerr << "Thread Pool Unknown Exception" << std::endl;
            }
        }
    }

public:
    inline ThreadPool(size_t num_workers);
    inline ~ThreadPool();

    // 禁用拷贝
    inline ThreadPool(const ThreadPool&) = delete;
    inline ThreadPool& operator=(const ThreadPool&) = delete;

public:
    template<class F>
    inline void enqueue(F&& f) {   // 完美转发，能够处理临时对象和左值对象
        // 将任务加入到队列中
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.emplace(std::forward<F>(f));
        }

        // 唤醒一个任务线程来接收任务
        m_condition.notify_one();
    }

    template<class F, typename ...Args>
    inline void emplace(F&& f, Args&& ...args) {   // 完美转发，能够处理临时对象和左值对象
        // 将任务加入到队列中
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            m_jobs.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        }

        // 唤醒一个任务线程来接收任务
        m_condition.notify_one();
    }
};

ThreadPool::ThreadPool(size_t num_workers)
{
    // 预分配内存，避免扩容
    m_workers.reserve(num_workers);

    for (size_t i = 0; i < num_workers; ++i) {
        m_workers.emplace_back(&ThreadPool::JobHandler, this);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    // 通知所有线程停止任务
    m_condition.notify_all();

    // 等待所有线程停止
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}


} // namespace web