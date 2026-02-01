//
// Created by Tristan on 1/30/26.
//

#include "../include/Chunk.hpp"

#include <algorithm>

#include "Blocks.hpp"
#include "Settings.hpp"

constexpr int WATER_LEVEL = 62;
constexpr int BEACH_LEVEL = 63;

void ChunkHelper::generateChunkTerrain(const std::unique_ptr<Chunk> &chunk) {
    int chunkOffsetX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int chunkOffsetZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = chunkOffsetX + x;
            int wz = chunkOffsetZ + z;

            float surfaceY = (getSurfaceHeight(wx, 0.0f, wz));

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y == 0) {
                    chunk->blockPosition[x][y][z] = ID_BEDROCK;
                } else if (y <= surfaceY) {
                    chunk->blockPosition[x][y][z] = ID_STONE;
                } else {
                    chunk->blockPosition[x][y][z] = ID_AIR;
                }
                if (surfaceY == BEACH_LEVEL) {
                    chunk->blockPosition[x][(int)surfaceY][z] = ID_SAND;
                }
            }

            if (surfaceY < WATER_LEVEL) {
                chunk->blockPosition[x][(int) surfaceY][z] = ID_WATER;
            }
        }
    }
}

bool ChunkHelper::isSolidBlock(int x, int y, int z) {
    float scale = 0.05f;
    float density = noise.GetNoise(x * scale, y * scale, z * scale);

    density = (density + 1.0f) * 0.5f;

    return density > 0.5f;
}

bool ChunkHelper::shouldPlaceTree(int worldX, int worldZ) {
    float freq = 0.1f;
    float val = noise.GetNoise(worldX * freq, 0.0f, worldZ * freq);
    val = (val + 1.0f) / 2.0f;
    return val > 0.8f;
}

void ChunkHelper::generateTree(std::unique_ptr<Chunk> &chunk, int x, int y, int z) {
    int treeHeight = 4 + (rand() % 3);

    for (int ty = 1; ty <= treeHeight; ty++) {
        if (y + ty >= CHUNK_SIZE_Y) break;
        chunk->blockPosition[x][y + ty][z] = ID_OAK_WOOD;
    }

    int leafBase = y + treeHeight;

    for (int lx = -2; lx <= 2; lx++) {
        for (int ly = 0; ly <= 2; ly++) {
            for (int lz = -2; lz <= 2; lz++) {
                int wx = x + lx;
                int wy = leafBase + ly;
                int wz = z + lz;

                if (wx < 0 || wx >= CHUNK_SIZE_X ||
                    wz < 0 || wz >= CHUNK_SIZE_Z ||
                    wy < 0 || wy >= CHUNK_SIZE_Y)
                    continue;

                if (abs(lx) + abs(ly) + abs(lz) <= 3) {
                    if (chunk->blockPosition[wx][wy][wz] == ID_AIR) {
                        chunk->blockPosition[wx][wy][wz] = ID_OAK_LEAF;
                    }
                }
            }
        }
    }
}

float ChunkHelper::getChunkHeight(const Vector3 &worldPosition) {
    float scale = 0.f;
    float height = ChunkHelper::noise.GetNoise(
        worldPosition.x * scale,
        worldPosition.y * scale,
        worldPosition.z * scale
    );

    height = (height + 1.0f) * 0.5f;

    return height * 64.0f + 64.0f;
}

void ChunkHelper::initNoiseRenderer() {
    ChunkHelper::noise.SetSeed(Settings::worldSeed);
    ChunkHelper::noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
}

float ChunkHelper::getSurfaceHeight(int worldX, int y, int worldZ) {
    float density = 0.0f;
    float frequency = 1.5f; // base hills
    float amplitude = 8.0f; // height scale
    float persistence = 0.5f;
    int octaves = 5;

    for (int o = 0; o < octaves; o++) {
        float nx = worldX * frequency;
        float ny = y;
        float nz = worldZ * frequency;

        density += noise.GetNoise(nx, ny, nz) * amplitude;

        amplitude *= persistence;
        frequency *= 2.0f;
    }

    // Base terrain level
    float baseHeight = 64.0f;

    return density - (y - baseHeight); // >0 = solid block
}

std::unique_ptr<Chunk> ChunkHelper::generateChunkAsync(const Vector3 &chunkPosIndex) {
    auto chunk = std::make_unique<Chunk>();


    int chunkX = static_cast<int>(chunkPosIndex.x);
    int chunkZ = static_cast<int>(chunkPosIndex.z);
    chunk->chunkCoords.x = chunkX;
    chunk->chunkCoords.z = chunkZ;

    generateChunkTerrain(chunk);
    setBiomeFloor(chunk);
    populateTrees(chunk);
    // Chunk bounding box
    chunk->boundingBox.min = {(float) chunkX * CHUNK_SIZE_X, 0.0f, (float) chunkZ * CHUNK_SIZE_Z};
    chunk->boundingBox.max = {
        (float) chunkX * CHUNK_SIZE_X + CHUNK_SIZE_X, (float) CHUNK_SIZE_Y, (float) chunkZ * CHUNK_SIZE_Z + CHUNK_SIZE_Z
    };

    chunk->chunkId = (chunkX & 0xFFFF) | ((chunkZ & 0xFFFF) << 16);
    chunk->loaded = false;
    chunk->alpha = 0.0f;

    return chunk;
}

bool ChunkHelper::isCave(int worldX, int y, int worldZ) {
    float frequency = 0.08f; // higher frequency = smaller caves
    float threshold = 0.65f;

    float value = noise.GetNoise(worldX * frequency, y * frequency, worldZ * frequency);
    return value > threshold;
}

void ChunkHelper::populateTrees(std::unique_ptr<Chunk> &chunk) {
    int chunkOffsetX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int chunkOffsetZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int surfaceY = -1;

            // Find highest grass block
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (chunk->blockPosition[x][y][z] == ID_GRASS) {
                    surfaceY = y;
                    break;
                }
            }

            if (surfaceY < 0) continue;

            int wx = chunkOffsetX + x;
            int wz = chunkOffsetZ + z;

            if (!shouldPlaceTree(wx, wz)) continue;

            generateTree(chunk, x, surfaceY, z);
        }
    }
}

std::unique_ptr<Chunk> *ChunkHelper::getChunkFromWorld(int wx, int wz) {
    int cx = floor((float) wx / CHUNK_SIZE_X);
    int cz = floor((float) wz / CHUNK_SIZE_Z);

    ChunkCoord coord{cx, cz};
    auto it = activeChunks.find(coord);
    if (it == activeChunks.end()) return nullptr;

    return &it->second;
}

int ChunkHelper::getBlock(int wx, int wy, int wz) {
    const auto &chunk = *getChunkFromWorld(wx, wz);
    if (!chunk) return ID_AIR;

    int lx = wx - chunk->chunkCoords.x * CHUNK_SIZE_X;
    int lz = wz - chunk->chunkCoords.z * CHUNK_SIZE_Z;

    if (wy < 0 || wy >= CHUNK_SIZE_Y) return ID_AIR;

    return chunk->blockPosition[lx][wy][lz];
}

void ChunkHelper::setBlock(int wx, int wy, int wz, int id) {
    const auto &chunk = *getChunkFromWorld(wx, wz);
    if (!chunk) return;

    int lx = wx - chunk->chunkCoords.x * CHUNK_SIZE_X;
    int lz = wz - chunk->chunkCoords.z * CHUNK_SIZE_Z;

    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;

    chunk->blockPosition[lx][wy][lz] = id;
    chunk->dirty = true; // force mesh rebuild
}

 ChunkCoord ChunkHelper::worldToChunkCoord(int wx, int wz){
    auto conv = [](int v, int size) {
        return (v >= 0) ? (v / size) : ((v - size + 1) / size);
    };

    return {
        conv(wx, CHUNK_SIZE_X),
        conv(wz, CHUNK_SIZE_Z)
    };
}

void ChunkHelper::markChunkDirty(const ChunkCoord& coord){
    std::lock_guard<std::mutex> lock(activeChunksMutex);

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end()) return;

    Chunk* chunk = it->second.get();
    if (!chunk || chunk->dirty) return;

    chunk->dirty = true;
    chunkBuildQueue.push(std::move(it->second));
    activeChunks.erase(it);
}

void ChunkHelper::setBiomeFloor(const std::unique_ptr<Chunk> &chunk) {
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for(int y = CHUNK_SIZE_Y - 1; y > WATER_LEVEL; y--){
                if (chunk->blockPosition[x][y][z] == ID_AIR) { continue; }
                if (chunk->blockPosition[x][y][z] == ID_STONE) chunk->blockPosition[x][y][z] = ID_GRASS;
                if (chunk->blockPosition[x][y][z] == ID_GRASS && chunk->blockPosition[x][y+1][z] != ID_AIR) {
                    chunk->blockPosition[x][y][z] = ID_DIRT;
                }
            }
        }
    }
}


