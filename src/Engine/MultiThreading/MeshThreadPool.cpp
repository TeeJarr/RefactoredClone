//
// Created by Tristan on 2/1/26.
//

#include "MeshThreadPool.hpp"

MeshThreadPool::MeshThreadPool(int numThreads) {
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back([this]() {
            while (running) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    condition.wait(lock, [this]() { return !running || !tasks.empty(); });

                    if (!running && tasks.empty()) return;

                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

void MeshThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

void MeshThreadPool::shutdown() {
    running = false;
    condition.notify_all();
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
}

MeshThreadPool::~MeshThreadPool() { shutdown(); }
