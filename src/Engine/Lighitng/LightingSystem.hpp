//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
#define REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
#pragma once
#include "Common.hpp"
#include <Chunk/Chunk.hpp>
#include <Block/Blocks.hpp>

static const int dx1[6] = {0, 0, -1, 1, 0, 0};
static const int dy1[6] = {0, 0, 0, 0, 1, -1};
static const int dz1[6] = {-1, 1, 0, 0, 0, 0};

// In Renderer or separate LightingSystem class
class LightingSystem {
    public:
    // Light propagation queue
    struct LightNode {
        int x, y, z;
        int chunkX, chunkZ;
        uint8_t lightLevel;
    };

    private:
    static void propagateLightQueue(Chunk& chunk, std::queue<LightNode>& lightQueue);

    public:
    static void calculateSkyLight(Chunk& chunk);

    static void propagateSkyLight(Chunk& chunk);

    static void calculateBlockLight(Chunk& chunk);

    static uint8_t getBlockLightEmission(BlockIds id);

    static void updateLightingAfterBlockBreak(Chunk& chunk, int x, int y, int z);

    static void calculateChunkLighting(Chunk& chunk);

    static void propagateLightAcrossChunks(Chunk& chunk);

    static void spreadLightFromNeighbor(Chunk& chunk, Chunk& neighbor, int edge);

    static int getWorldLightLevel(int worldX, int worldY, int worldZ);

    public:
    static void debugChunkLight(Chunk& chunk) {
        int nonZeroSky = 0;
        int maxSky = 0;
        int minNonZeroSky = 15;

        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    int sky = chunk.getSkyLight(x, y, z);
                    if (sky > 0) {
                        nonZeroSky++;
                        maxSky = std::max(maxSky, sky);
                        minNonZeroSky = std::min(minNonZeroSky, sky);
                    }
                }
            }
        }

#ifndef NDEBUG
        printf("Chunk (%d, %d): nonZeroSky=%d, max=%d, min=%d\n", chunk.chunkCoords.x,
               chunk.chunkCoords.z, nonZeroSky, maxSky, minNonZeroSky);
#endif
    }
};

#endif // REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
