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
    MeshThreadPool(int numThreads = 2);
    void submit(std::function<void()> task);
    void shutdown();
    ~MeshThreadPool();
};

static std::unique_ptr<MeshThreadPool> g_meshThreadPool;

#endif