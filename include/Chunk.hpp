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

#include "Blocks.hpp"


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

struct TerrainParams {
    // Large landmasses (continents/oceans)
    float continentFreq = 0.03f;
    float continentAmp = 35.0f;

    // Biome-scale variation
    float regionFreq = 0.08f;
    float regionAmp = 20.0f;

    // Hills
    float hillFreq = 0.1f;
    float hillAmp = 18.0f;

    // Small bumps
    float detailFreq = 0.6f;
    float detailAmp = 6.0f;

    // Surface roughness
    float microFreq = 0.12f;
    float microAmp = 2.0f;

    // Mountains
    float mountainFreq = 0.5f;
    float mountainThreshold = 0.4f;
    float mountainAmp = 55.0f;

    // General
    float baseHeight = 64.0f;
    float seaLevel = 50.0f;
};

inline TerrainParams terrainParams;

// In Chunk.hpp - replace the existing BiomeType enum and Biome struct

enum BiomeType {
    BIOME_OCEAN,
    BIOME_BEACH,
    BIOME_DESERT,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_HILLS,
    BIOME_MOUNTAINS,
    NUM_BIOMES
};

struct Biome {
    BiomeType type;
    float baseHeight;
    float heightVariation;
    float hillFrequency;
    float hillAmplitude;
    int surfaceBlock;
    int subsurfaceBlock;
    float treeChance; // 0 = no trees, higher = more trees
    bool hasRivers;
};

inline std::unordered_map<BiomeType, Biome> biomes = {
    {BIOME_OCEAN, {BIOME_OCEAN, 35.0f, 8.0f, 0.01f, 3.0f, ID_SAND, ID_SAND, 0.0f, false}},
    {BIOME_BEACH, {BIOME_BEACH, 50.0f, 3.0f, 0.02f, 2.0f, ID_SAND, ID_SAND, 0.0f, false}},
    {BIOME_DESERT, {BIOME_DESERT, 62.0f, 10.0f, 0.015f, 8.0f, ID_SAND, ID_SAND, 0.0f, false}},
    {BIOME_PLAINS, {BIOME_PLAINS, 64.0f, 6.0f, 0.02f, 4.0f, ID_GRASS, ID_DIRT, 0.003f, true}},
    {BIOME_FOREST, {BIOME_FOREST, 66.0f, 10.0f, 0.025f, 6.0f, ID_GRASS, ID_DIRT, 0.04f, true}},
    {BIOME_HILLS, {BIOME_HILLS, 72.0f, 25.0f, 0.03f, 15.0f, ID_GRASS, ID_DIRT, 0.01f, true}},
    {BIOME_MOUNTAINS, {BIOME_MOUNTAINS, 85.0f, 55.0f, 0.04f, 30.0f, ID_STONE, ID_STONE, 0.005f, false}},
};

// In ChunkMeshBuffers, add reserve:
struct ChunkMeshBuffers {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    std::vector<unsigned short> indices;

    void reserve(size_t expectedFaces) {
        size_t verts = expectedFaces * 4;
        vertices.reserve(verts * 3);
        normals.reserve(verts * 3);
        texcoords.reserve(verts * 2);
        colors.reserve(verts * 4);
        indices.reserve(expectedFaces * 6);
    }

    void clear() {
        vertices.clear();
        normals.clear();
        texcoords.clear();
        colors.clear();
        indices.clear();
    }
};

// In buildChunkMeshes:
struct ChunkMeshTriple {
    ChunkMeshBuffers opaque;
    ChunkMeshBuffers translucent;
    ChunkMeshBuffers water;
};

struct Chunk {
    int blockPosition[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    int surfaceHeight[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    int biomeMap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    ChunkCoord chunkCoords{};
    BoundingBox boundingBox{};

    // Three separate models
    mutable Model opaqueModel = {0};
    mutable Model translucentModel = {0}; // Leaves, glass, etc.
    mutable Model waterModel = {0}; // Water only

    mutable bool meshBuilt = false;
    mutable Material material = {0};

    int chunkId;

    std::atomic<bool> loaded = false;
    float alpha = 0.0f;
    bool dirty = true;

    uint8_t skyLight[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    uint8_t blockLight[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

    // Helper to get combined light level (0-15)
    int getLightLevel(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE_X ||
            y < 0 || y >= CHUNK_SIZE_Y ||
            z < 0 || z >= CHUNK_SIZE_Z)
            return 15;

        int sky = skyLight[x][y][z];
        int block = blockLight[x][y][z];

        // DEBUG
        static int debugCount = 0;
        if (debugCount++ < 20 && (sky > 0 || block > 0)) {
            printf("getLightLevel(%d,%d,%d): sky=%d, block=%d\n", x, y, z, sky, block);
        }

        return std::max(sky, block);
    }

    std::unique_ptr<ChunkMeshTriple> pendingMeshData;
    std::atomic<bool> meshReady{false};
    std::atomic<bool> meshBuilding{false};
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
    inline FastNoiseLite terrainNoise;
    inline FastNoiseLite biomeNoise;
    inline FastNoiseLite temperatureNoise;
    inline FastNoiseLite treeNoise;
    inline FastNoiseLite caveNoise;
    inline FastNoiseLite humidityNoise;
    inline FastNoiseLite continentalnessNoise;

    static BlockIds getWorldBlock(int worldX, int worldY, int worldZ);

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

    BiomeType getBiome(int wx, int wz);

    float getChunkHeight(const Vector3 &worldPosition);

    void initNoiseRenderer();

    inline int chunkCount = 0;

    void generateChunkBlocks(Chunk &chunk);

    float getBlockyHeight(const Vector3 &worldPos);

    float getSurfaceHeight(int wx, int wz);

    bool isSolidBlock(int x, int y, int z);

    bool shouldPlaceTree(int worldX, int worldZ);

    void generateTree(int x, int y, int z);

    std::unique_ptr<Chunk> generateChunkAsync(const Vector3 &chunkPosIndex);

    bool isCave(int worldX, int y, int worldZ);

    void populateTrees(Chunk &chunk);

    void placeWater(const std::unique_ptr<Chunk> &chunk, int chunkOffsetX, int chunkOffsetZ);

    Chunk *getChunkFromWorld(int wx, int wz);

    int getBlock(int wx, int wy, int wz);

    void setBlock(int wx, int wy, int wz, int id);

    void generateChunkTerrain(const std::unique_ptr<Chunk> &chunk);

    ChunkCoord worldToChunkCoord(int wx, int wz);

    void markChunkDirty(const ChunkCoord &coord);

    void setBiomeFloor(const std::unique_ptr<Chunk> &chunk);

    int WorldToChunk(int w);

    int WorldToLocal(int w);

    float LERP(float a, float b, float t);

    Biome getBiomeType(float biomeFactor);

    float getBiomeFactor(int wx, int wz);


    BiomeType getBiomeAt(int wx, int wz);

    float getTemperature(int wx, int wz);

    float getHumidity(int wx, int wz);

    float getContinentalness(int wx, int wz);

    // In Blocks.hpp or a new Biomes.hpp


    static ChunkMeshTriple buildChunkMeshes(const Chunk &chunk);

    static void drawChunkWater(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera);

    inline Color getBiomeGrassTint(BiomeType biome) {
        switch (biome) {
            case BIOME_DESERT:
                return {190, 180, 110, 255}; // Yellowish dry grass
            case BIOME_FOREST:
                return {80, 140, 50, 255}; // Dark green
            case BIOME_PLAINS:
                return {105, 175, 59, 255}; // Normal green
            case BIOME_HILLS:
                return {90, 160, 60, 255}; // Slightly darker
            case BIOME_MOUNTAINS:
                return {120, 150, 80, 255}; // Grayish green
            case BIOME_OCEAN:
            case BIOME_BEACH:
            default:
                return {105, 175, 59, 255};
        }
    }

    inline Color getBiomeWaterTint(BiomeType biome) {
        switch (biome) {
            case BIOME_DESERT:
                return {80, 150, 200, 180}; // Lighter blue
            case BIOME_FOREST:
                return {50, 90, 180, 180}; // Darker blue
            case BIOME_OCEAN:
                return {40, 80, 200, 180}; // Deep blue
            default:
                return {60, 100, 255, 180};
        }
    }

    inline Color getBiomeLeafTint(BiomeType biome) {
        switch (biome) {
            case BIOME_DESERT:
                return {140, 160, 80, 255}; // Olive/dry
            case BIOME_FOREST:
                return {60, 120, 40, 255}; // Deep green
            case BIOME_PLAINS:
                return {80, 140, 50, 255}; // Normal
            default:
                return {80, 140, 50, 255};
        }
    }
}


#endif //REFACTOREDCLONE_CHUNK_HPP
