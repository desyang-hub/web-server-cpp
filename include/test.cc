#include "thread_pool.h"
#include "log/log.h"

#include <chrono>

#include <thread>

const int num_workers = 4;

void job_handler(int id) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    LOG_INFO << "task id: " << id << " finish!" << endl;
}

int main() {

    
    ThreadPool pool(4);

    for (int i = 1; i <= 8; ++i) {
        pool.enqueue(std::bind(job_handler, i));
    }

    return 0;
}