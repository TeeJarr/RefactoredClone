//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_MESHTHREADPOOL_HPP
#define REFACTOREDCLONE_MESHTHREADPOOL_HPP

#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>


// In Renderer.hpp or a new header
class MeshThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex taskMutex;
    std::condition_variable condition;
    std::atomic<bool> running{true};

public:
    MeshThreadPool(int numThreads = 2) {
        for (int i = 0; i < numThreads; i++) {
            workers.emplace_back([this]() {
                while (running) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(taskMutex);
                        condition.wait(lock, [this]() {
                            return !running || !tasks.empty();
                        });

                        if (!running && tasks.empty()) return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    void shutdown() {
        running = false;
        condition.notify_all();
        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
    }

    ~MeshThreadPool() {
        shutdown();
    }
};

// Global instance (in Renderer.cpp)
static std::unique_ptr<MeshThreadPool> g_meshThreadPool;

#endif