//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
#define REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
#pragma once
#include <Chunk.hpp>
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

    static void calculateSkyLight(Chunk &chunk) {
        memset(chunk.skyLight, 0, sizeof(chunk.skyLight));

        // Step 1: Downward pass
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                int lightLevel = 15;

                for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                    BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);

                    if (id == ID_AIR || isBlockTranslucent(id)) {
                        chunk.skyLight[x][y][z] = lightLevel;
                    } else {
                        chunk.skyLight[x][y][z] = 0;
                        lightLevel = 0;
                    }
                }
            }
        }

        // DEBUG: Count blocks that need horizontal propagation (light < 15 but air)
        int needsSpread = 0;
        int underBlocks = 0;
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                    if ((id == ID_AIR || isBlockTranslucent(id)) && chunk.skyLight[x][y][z] == 0) {
                        needsSpread++;
                    }
                    if (id == ID_AIR && chunk.skyLight[x][y][z] == 0) {
                        underBlocks++;
                    }
                }
            }
        }
        printf("DEBUG: Air blocks with 0 light (need spread): %d, under blocks: %d\n", needsSpread, underBlocks);

        propagateSkyLight(chunk);
    }

    static void propagateSkyLight(Chunk &chunk) {
        std::queue<LightNode> lightQueue;

        // Seed queue with all blocks that have light
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    if (chunk.skyLight[x][y][z] > 0) {
                        lightQueue.push({
                            x, y, z, chunk.chunkCoords.x,
                            chunk.chunkCoords.z, chunk.skyLight[x][y][z]
                        });
                    }
                }
            }
        }

        printf("DEBUG: Seeded queue with %zu light sources\n", lightQueue.size());

        int iterations = 0;

        // BFS propagation
        while (!lightQueue.empty()) {
            LightNode node = lightQueue.front();
            lightQueue.pop();
            iterations++;

            if (node.lightLevel <= 1) continue;

            // Check all 6 directions
            for (int f = 0; f < 6; f++) {
                int nx = node.x + dx1[f];
                int ny = node.y + dy1[f];
                int nz = node.z + dz1[f];

                // Bounds check
                if (nx < 0 || nx >= CHUNK_SIZE_X ||
                    ny < 0 || ny >= CHUNK_SIZE_Y ||
                    nz < 0 || nz >= CHUNK_SIZE_Z)
                    continue;

                BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);

                // Skip opaque blocks
                if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

                uint8_t newLight = node.lightLevel - 1;

                // Only update if this gives more light
                if (newLight > chunk.skyLight[nx][ny][nz]) {
                    chunk.skyLight[nx][ny][nz] = newLight;
                    lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
                }
            }
        }

        printf("DEBUG: BFS completed after %d iterations\n", iterations);
    }

    static void calculateBlockLight(Chunk &chunk) {
        // Initialize block light from light-emitting blocks
        std::queue<LightNode> lightQueue;

        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                    uint8_t emission = getBlockLightEmission(id);

                    if (emission > 0) {
                        chunk.blockLight[x][y][z] = emission;
                        lightQueue.push({
                            x, y, z, chunk.chunkCoords.x,
                            chunk.chunkCoords.z, emission
                        });
                    } else {
                        chunk.blockLight[x][y][z] = 0;
                    }
                }
            }
        }

        // Propagate block light
        while (!lightQueue.empty()) {
            LightNode node = lightQueue.front();
            lightQueue.pop();

            if (node.lightLevel <= 1) continue;

            for (int f = 0; f < 6; f++) {
                int nx = node.x + dx1[f];
                int ny = node.y + dy1[f];
                int nz = node.z + dz1[f];

                if (nx < 0 || nx >= CHUNK_SIZE_X ||
                    ny < 0 || ny >= CHUNK_SIZE_Y ||
                    nz < 0 || nz >= CHUNK_SIZE_Z)
                    continue;

                BlockIds neighborId = static_cast<BlockIds>(
                    chunk.blockPosition[nx][ny][nz]);

                if (neighborId != ID_AIR && !isBlockTranslucent(neighborId))
                    continue;

                uint8_t newLight = node.lightLevel - 1;

                if (newLight > chunk.blockLight[nx][ny][nz]) {
                    chunk.blockLight[nx][ny][nz] = newLight;
                    lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
                }
            }
        }
    }

    // Define which blocks emit light
    static uint8_t getBlockLightEmission(BlockIds id) {
        switch (id) {
            case ID_TORCH: return 14; // if you have torches
            case ID_LAVA: return 15; // if you have lava
            case ID_GLOWSTONE: return 15; // etc.
            default: return 0;
        }
    }

    static void updateLightingAfterBlockBreak(Chunk &chunk, int x, int y, int z) {
        // When a block is removed, light can now flow into that spot

        // 1. Check all 6 neighbors for their light levels
        int maxNeighborLight = 0;

        for (int f = 0; f < 6; f++) {
            int nx = x + dx1[f];
            int ny = y + dy1[f];
            int nz = z + dz1[f];

            if (nx >= 0 && nx < CHUNK_SIZE_X &&
                ny >= 0 && ny < CHUNK_SIZE_Y &&
                nz >= 0 && nz < CHUNK_SIZE_Z) {
                maxNeighborLight = std::max(maxNeighborLight, (int) chunk.skyLight[nx][ny][nz]);
            }
        }

        // 2. Also check if this block now has direct sky access
        bool hasSkyAbove = true;
        for (int checkY = y + 1; checkY < CHUNK_SIZE_Y; checkY++) {
            BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][checkY][z]);
            if (id != ID_AIR && !isBlockTranslucent(id)) {
                hasSkyAbove = false;
                break;
            }
        }

        if (hasSkyAbove) {
            chunk.skyLight[x][y][z] = 15;
        } else {
            chunk.skyLight[x][y][z] = std::max(0, maxNeighborLight - 1);
        }

        // 3. Propagate from this point
        std::queue<LightNode> lightQueue;
        lightQueue.push({x, y, z, chunk.chunkCoords.x, chunk.chunkCoords.z, chunk.skyLight[x][y][z]});

        // ... same BFS propagation as before
    }

    // Call this after a chunk is generated or a block changes
    static void calculateChunkLighting(Chunk &chunk) {
        calculateSkyLight(chunk);
        calculateBlockLight(chunk);

        // Propagate to/from neighbors
        propagateLightAcrossChunks(chunk);
    }

    static void propagateLightAcrossChunks(Chunk &chunk) {
        std::queue<LightNode> lightQueue;

        ChunkCoord coord = chunk.chunkCoords;

        // Check all 4 horizontal neighbors
        ChunkCoord neighbors[4] = {
            {coord.x - 1, coord.z},
            {coord.x + 1, coord.z},
            {coord.x, coord.z - 1},
            {coord.x, coord.z + 1}
        };

        // Seed queue from chunk edges
        // Left edge (x = 0) - check neighbor to the left
        if (ChunkHelper::activeChunks.contains(neighbors[0])) {
            auto &leftChunk = ChunkHelper::activeChunks.at(neighbors[0]);
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    // Light from left chunk's right edge into our left edge
                    int neighborLight = leftChunk->skyLight[CHUNK_SIZE_X - 1][y][z];
                    if (neighborLight > 1 && neighborLight - 1 > chunk.skyLight[0][y][z]) {
                        auto id = static_cast<BlockIds>(chunk.blockPosition[0][y][z]);
                        if (id == ID_AIR || isBlockTranslucent(id)) {
                            chunk.skyLight[0][y][z] = neighborLight - 1;
                            lightQueue.push({0, y, z, coord.x, coord.z, (uint8_t) (neighborLight - 1)});
                        }
                    }
                }
            }
        }

        // Right edge (x = CHUNK_SIZE_X - 1)
        if (ChunkHelper::activeChunks.contains(neighbors[1])) {
            auto &rightChunk = ChunkHelper::activeChunks.at(neighbors[1]);
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    int neighborLight = rightChunk->skyLight[0][y][z];
                    if (neighborLight > 1 && neighborLight - 1 > chunk.skyLight[CHUNK_SIZE_X - 1][y][z]) {
                        BlockIds id = static_cast<BlockIds>(chunk.blockPosition[CHUNK_SIZE_X - 1][y][z]);
                        if (id == ID_AIR || isBlockTranslucent(id)) {
                            chunk.skyLight[CHUNK_SIZE_X - 1][y][z] = neighborLight - 1;
                            lightQueue.push({CHUNK_SIZE_X - 1, y, z, coord.x, coord.z, (uint8_t) (neighborLight - 1)});
                        }
                    }
                }
            }
        }

        // Front edge (z = 0)
        if (ChunkHelper::activeChunks.count(neighbors[2])) {
            auto &frontChunk = ChunkHelper::activeChunks.at(neighbors[2]);
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int x = 0; x < CHUNK_SIZE_X; x++) {
                    int neighborLight = frontChunk->skyLight[x][y][CHUNK_SIZE_Z - 1];
                    if (neighborLight > 1 && neighborLight - 1 > chunk.skyLight[x][y][0]) {
                        BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][0]);
                        if (id == ID_AIR || isBlockTranslucent(id)) {
                            chunk.skyLight[x][y][0] = neighborLight - 1;
                            lightQueue.push({x, y, 0, coord.x, coord.z, (uint8_t) (neighborLight - 1)});
                        }
                    }
                }
            }
        }

        // Back edge (z = CHUNK_SIZE_Z - 1)
        if (ChunkHelper::activeChunks.count(neighbors[3])) {
            auto &backChunk = ChunkHelper::activeChunks.at(neighbors[3]);
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int x = 0; x < CHUNK_SIZE_X; x++) {
                    int neighborLight = backChunk->skyLight[x][y][0];
                    if (neighborLight > 1 && neighborLight - 1 > chunk.skyLight[x][y][CHUNK_SIZE_Z - 1]) {
                        BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][CHUNK_SIZE_Z - 1]);
                        if (id == ID_AIR || isBlockTranslucent(id)) {
                            chunk.skyLight[x][y][CHUNK_SIZE_Z - 1] = neighborLight - 1;
                            lightQueue.push({x, y, CHUNK_SIZE_Z - 1, coord.x, coord.z, (uint8_t) (neighborLight - 1)});
                        }
                    }
                }
            }
        }

        // Now propagate inward using BFS (same as before)
        propagateLightQueue(chunk, lightQueue);
    }

    // Call this AFTER all nearby chunks have initial lighting calculated
    // In LightingSystem class (make sure dx, dy, dz are accessible)
    static constexpr int dx[6] = {0, 0, -1, 1, 0, 0};
    static constexpr int dy[6] = {0, 0, 0, 0, 1, -1};
    static constexpr int dz[6] = {-1, 1, 0, 0, 0, 0};

    static void spreadLightFromNeighbor(Chunk &chunk, Chunk &neighbor, int edge) {
        printf("DEBUG: spreadLightFromNeighbor - chunk(%d,%d) edge=%d\n",
               chunk.chunkCoords.x, chunk.chunkCoords.z, edge);

        std::queue<LightNode> lightQueue;
        int edgeUpdates = 0;

        switch (edge) {
            case 0: // Neighbor is to our -X
                for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                    for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                        int neighborLight = neighbor.skyLight[CHUNK_SIZE_X - 1][y][z];
                        BlockIds id = static_cast<BlockIds>(chunk.blockPosition[0][y][z]);
                        if ((id == ID_AIR || isBlockTranslucent(id)) && neighborLight > 1) {
                            uint8_t newLight = neighborLight - 1;
                            if (newLight > chunk.skyLight[0][y][z]) {
                                chunk.skyLight[0][y][z] = newLight;
                                lightQueue.push({0, y, z, 0, 0, newLight});
                                edgeUpdates++;
                            }
                        }
                    }
                }
                break;
                // ... same for other cases, add edgeUpdates++
        }

        printf("DEBUG: Edge updates: %d, queue size: %zu\n", edgeUpdates, lightQueue.size());

        int propagated = 0;
        while (!lightQueue.empty()) {
            LightNode node = lightQueue.front();
            lightQueue.pop();

            if (node.lightLevel <= 1) continue;

            for (int f = 0; f < 6; f++) {
                int nx = node.x + dx[f];
                int ny = node.y + dy[f];
                int nz = node.z + dz[f];

                if (nx < 0 || nx >= CHUNK_SIZE_X ||
                    ny < 0 || ny >= CHUNK_SIZE_Y ||
                    nz < 0 || nz >= CHUNK_SIZE_Z)
                    continue;

                BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);
                if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

                uint8_t newLight = node.lightLevel - 1;
                if (newLight > chunk.skyLight[nx][ny][nz]) {
                    chunk.skyLight[nx][ny][nz] = newLight;
                    lightQueue.push({nx, ny, nz, 0, 0, newLight});
                    propagated++;
                }
            }
        }

        printf("DEBUG: Total propagated: %d\n", propagated);
    }

    // Add this to ChunkHelper or a similar utility class
    static int getWorldLightLevel(int worldX, int worldY, int worldZ) {
        if (worldY < 0 || worldY >= CHUNK_SIZE_Y) {
            return (worldY >= CHUNK_SIZE_Y) ? 15 : 0; // Above world = bright, below = dark
        }

        ChunkCoord coord = ChunkHelper::worldToChunkCoord(worldX, worldZ);

        auto it = ChunkHelper::activeChunks.find(coord);
        if (it == ChunkHelper::activeChunks.end() || !it->second) {
            return 15; // Chunk not loaded, assume bright (or could return 0)
        }

        int localX = ChunkHelper::WorldToLocal(worldX);
        int localZ = ChunkHelper::WorldToLocal(worldZ);

        return it->second->getLightLevel(localX, worldY, localZ);
    }

private:
    static void propagateLightQueue(Chunk &chunk, std::queue<LightNode> &lightQueue) {
        while (!lightQueue.empty()) {
            LightNode node = lightQueue.front();
            lightQueue.pop();

            if (node.lightLevel <= 1) continue;

            for (int f = 0; f < 6; f++) {
                int nx = node.x + dx1[f];
                int ny = node.y + dy1[f];
                int nz = node.z + dz1[f];

                if (nx < 0 || nx >= CHUNK_SIZE_X ||
                    ny < 0 || ny >= CHUNK_SIZE_Y ||
                    nz < 0 || nz >= CHUNK_SIZE_Z)
                    continue;

                BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);
                if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

                uint8_t newLight = node.lightLevel - 1;
                if (newLight > chunk.skyLight[nx][ny][nz]) {
                    chunk.skyLight[nx][ny][nz] = newLight;
                    lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
                }
            }
        }
    }

public:
    // Add this temporary debug function
    static void debugChunkLight(Chunk &chunk) {
        int nonZeroSky = 0;
        int maxSky = 0;
        int minNonZeroSky = 15;

        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    int sky = chunk.skyLight[x][y][z];
                    if (sky > 0) {
                        nonZeroSky++;
                        maxSky = std::max(maxSky, sky);
                        minNonZeroSky = std::min(minNonZeroSky, sky);
                    }
                }
            }
        }

        printf("Chunk (%d, %d): nonZeroSky=%d, max=%d, min=%d\n",
               chunk.chunkCoords.x, chunk.chunkCoords.z,
               nonZeroSky, maxSky, minNonZeroSky);
    }
};

#endif //REFACTOREDCLONE_LIGHTINGSYSTEM_HPP
