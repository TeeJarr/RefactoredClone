//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_CHUNK_HPP
#define REFACTOREDCLONE_CHUNK_HPP
#pragma once

#include <raylib.h>
#include <unordered_map>
#include <FastNoiseLite.h>
#include <vector>
#include <atomic>


#include <queue>
#include <mutex>
#include <memory>

#include <queue>
#include <mutex>
#include <memory>
#include <thread>
#include <unordered_set>
#include <optional>


struct ChunkCoord {
    int x, z;

    bool operator==(const ChunkCoord &other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord &c) const noexcept {
        std::size_t h1 = std::hash<int>{}(c.x);
        std::size_t h2 = std::hash<int>{}(c.z);
        return h1 ^ (h2 << 1);
    }
};

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;

struct Chunk {
    int blockPosition[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    ChunkCoord chunkCoords{};
    BoundingBox boundingBox{};

    mutable Mesh mesh = {0};
    mutable Model model = {0};
    mutable bool meshBuilt = false;
    mutable Material material = {0};

    int chunkId;

    std::atomic<bool> loaded = false;
    float alpha = 0.0f;
    bool dirty = true;
};

struct ChunkMeshBuffers {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    std::vector<unsigned short> indices;
};


template<typename T>
struct ThreadSafeQueue {
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

    void push(T item) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(std::move(item));
        cv.notify_one();
    }

    bool try_pop(T &out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return false;
        out = std::move(queue.front());
        queue.pop();
        return true;
    }

    std::optional<T> front() {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) return std::nullopt; // or default T
        return queue.front();
    }

    T wait_pop(std::atomic<bool> &running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return !queue.empty() || !running.load(); });

        if (!running.load()) return T{}; // return default value if shutting down

        T item = std::move(queue.front());
        queue.pop();
        return item;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    void notifyAll() {
        cv.notify_all();
    }
};


namespace ChunkHelper {
    inline FastNoiseLite noise;
    inline ThreadSafeQueue<std::unique_ptr<Chunk> > chunkBuildQueue;

    static ThreadSafeQueue<std::unique_ptr<Chunk> > chunkGenQueue; // queue of std::unique_ptr<Chunk> for CPU generation

    inline ThreadSafeQueue<ChunkCoord> chunkRequestQueue;
    inline std::mutex chunkRequestQueueMtx;

    inline std::unordered_set<ChunkCoord, ChunkCoordHash> chunkRequestSet;
    inline std::mutex chunkRequestSetMutex;

    // GPU-ready chunks only
    inline std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> activeChunks;
    inline std::mutex activeChunksMutex;

    inline const unsigned int WORKER_COUNT = std::thread::hardware_concurrency();
    inline std::thread chunkWorker;
    inline std::atomic<bool> workerRunning = false;
    inline std::condition_variable chunkRequestCV;

    std::unique_ptr<Chunk> generateChunk(const Vector3 &chunkPosIndex);

    float getChunkHeight(const Vector3 &worldPosition);

    void initNoiseRenderer();

    inline int chunkCount = 0;

    void generateChunkBlocks(Chunk &chunk);

    float getBlockyHeight(const Vector3 &worldPos);

    float getSurfaceHeight(int worldX, int y, int worldZ);

    bool isSolidBlock(int x, int y, int z);

    bool shouldPlaceTree(int worldX, int worldZ);

    void generateTree(std::unique_ptr<Chunk> &chunk, int x, int y, int z);

    std::unique_ptr<Chunk> generateChunkAsync(const Vector3 &chunkPosIndex);

    bool isCave(int worldX, int y, int worldZ);

    void populateTrees(std::unique_ptr<Chunk> &chunk);

    void placeWater(const std::unique_ptr<Chunk> &chunk, int chunkOffsetX, int chunkOffsetZ);

    std::unique_ptr<Chunk> *getChunkFromWorld(int wx, int wz);

    int getBlock(int wx, int wy, int wz);

    void setBlock(int wx, int wy, int wz, int id);

    void generateChunkTerrain(const std::unique_ptr<Chunk> &chunk);

    ChunkCoord worldToChunkCoord(int wx, int wz);

    void markChunkDirty(const ChunkCoord& coord);

    void setBiomeFloor(const std::unique_ptr<Chunk>& chunk);
}


#endif //REFACTOREDCLONE_CHUNK_HPP
