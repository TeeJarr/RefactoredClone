//
// Created by Tristan on 2/1/26.
//

#include "LightingSystem.hpp"

void LightingSystem::calculateSkyLight(Chunk& chunk) {
    memset(chunk.packedLight, 0, sizeof(chunk.packedLight));

    // Step 1: Downward pass
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int lightLevel = 15;

            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);

                if (id == ID_AIR || isBlockTranslucent(id)) {
                    chunk.setSkyLight(x, y, z, lightLevel);
                } else {
                    chunk.setSkyLight(x, y, z, 0);
                    lightLevel = 0;
                }
            }
        }
    }

    int needsSpread = 0;
    int underBlocks = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                if ((id == ID_AIR || isBlockTranslucent(id)) && chunk.getSkyLight(x, y, z) == 0) {
                    needsSpread++;
                }
                if (id == ID_AIR && chunk.getSkyLight(x, y, z) == 0) {
                    underBlocks++;
                }
            }
        }
    }
#ifndef NDEBUG
    printf("DEBUG: Air blocks with 0 light (need spread): %d, under blocks: %d\n", needsSpread,
           underBlocks);
#endif

    propagateSkyLight(chunk);
}

void LightingSystem::propagateSkyLight(Chunk& chunk) {
    std::queue<LightNode> lightQueue;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                uint8_t sky = chunk.getSkyLight(x, y, z);
                if (sky > 0) {
                    lightQueue.push({x, y, z, chunk.chunkCoords.x, chunk.chunkCoords.z, sky});
                }
            }
        }
    }

#ifndef NDEBUG
    printf("DEBUG: Seeded queue with %zu light sources\n", lightQueue.size());
#endif

    int iterations = 0;

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();
        iterations++;

        if (node.lightLevel <= 1) continue;

        for (int f = 0; f < 6; f++) {
            int nx = node.x + dx1[f];
            int ny = node.y + dy1[f];
            int nz = node.z + dz1[f];

            if (nx < 0 || nx >= CHUNK_SIZE_X || ny < 0 || ny >= CHUNK_SIZE_Y || nz < 0 ||
                nz >= CHUNK_SIZE_Z)
                continue;

            BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);

            if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

            uint8_t newLight = node.lightLevel - 1;

            if (newLight > chunk.getSkyLight(nx, ny, nz)) {
                chunk.setSkyLight(nx, ny, nz, newLight);
                lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
            }
        }
    }

#ifndef NDEBUG
    printf("DEBUG: BFS completed after %d iterations\n", iterations);
#endif
}

void LightingSystem::calculateBlockLight(Chunk& chunk) {
    std::queue<LightNode> lightQueue;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                uint8_t emission = getBlockLightEmission(id);

                if (emission > 0) {
                    chunk.setBlockLight(x, y, z, emission);
                    lightQueue.push({x, y, z, chunk.chunkCoords.x, chunk.chunkCoords.z, emission});
                } else {
                    chunk.setBlockLight(x, y, z, 0);
                }
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        if (node.lightLevel <= 1) continue;

        for (int f = 0; f < 6; f++) {
            int nx = node.x + dx1[f];
            int ny = node.y + dy1[f];
            int nz = node.z + dz1[f];

            if (nx < 0 || nx >= CHUNK_SIZE_X || ny < 0 || ny >= CHUNK_SIZE_Y || nz < 0 ||
                nz >= CHUNK_SIZE_Z)
                continue;

            BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);

            if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

            uint8_t newLight = node.lightLevel - 1;

            if (newLight > chunk.getBlockLight(nx, ny, nz)) {
                chunk.setBlockLight(nx, ny, nz, newLight);
                lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
            }
        }
    }
}

uint8_t LightingSystem::getBlockLightEmission(BlockIds id) {
    switch (id) {
        case ID_TORCH:
            return 14;
        case ID_LAVA:
            return 15;
        case ID_GLOWSTONE:
            return 15;
        default:
            return 0;
    }
}

void LightingSystem::updateLightingAfterBlockBreak(Chunk& chunk, int x, int y, int z) {
    int maxNeighborLight = 0;

    for (int f = 0; f < 6; f++) {
        int nx = x + dx1[f];
        int ny = y + dy1[f];
        int nz = z + dz1[f];

        if (nx >= 0 && nx < CHUNK_SIZE_X && ny >= 0 && ny < CHUNK_SIZE_Y && nz >= 0 &&
            nz < CHUNK_SIZE_Z) {
            maxNeighborLight = std::max(maxNeighborLight, (int)chunk.getSkyLight(nx, ny, nz));
        }
    }

    bool hasSkyAbove = true;
    for (int checkY = y + 1; checkY < CHUNK_SIZE_Y; checkY++) {
        BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][checkY][z]);
        if (id != ID_AIR && !isBlockTranslucent(id)) {
            hasSkyAbove = false;
            break;
        }
    }

    if (hasSkyAbove) {
        chunk.setSkyLight(x, y, z, 15);
    } else {
        chunk.setSkyLight(x, y, z, std::max(0, maxNeighborLight - 1));
    }

    std::queue<LightNode> lightQueue;
    lightQueue.push({x, y, z, chunk.chunkCoords.x, chunk.chunkCoords.z, chunk.getSkyLight(x, y, z)});
}

void LightingSystem::calculateChunkLighting(Chunk& chunk) {
    calculateSkyLight(chunk);
    calculateBlockLight(chunk);

    propagateLightAcrossChunks(chunk);
}

void LightingSystem::propagateLightAcrossChunks(Chunk& chunk) {
    std::queue<LightNode> lightQueue;

    ChunkCoord coord = chunk.chunkCoords;

    ChunkCoord neighbors[4] = {{coord.x - 1, coord.z},
                               {coord.x + 1, coord.z},
                               {coord.x, coord.z - 1},
                               {coord.x, coord.z + 1}};

    if (ChunkHelper::activeChunks.contains(neighbors[0])) {
        auto& leftChunk = ChunkHelper::activeChunks.at(neighbors[0]);
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                int neighborLight = leftChunk->getSkyLight(CHUNK_SIZE_X - 1, y, z);
                if (neighborLight > 1 && neighborLight - 1 > chunk.getSkyLight(0, y, z)) {
                    auto id = static_cast<BlockIds>(chunk.blockPosition[0][y][z]);
                    if (id == ID_AIR || isBlockTranslucent(id)) {
                        chunk.setSkyLight(0, y, z, neighborLight - 1);
                        lightQueue.push({0, y, z, coord.x, coord.z, (uint8_t)(neighborLight - 1)});
                    }
                }
            }
        }
    }

    if (ChunkHelper::activeChunks.contains(neighbors[1])) {
        auto& rightChunk = ChunkHelper::activeChunks.at(neighbors[1]);
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                int neighborLight = rightChunk->getSkyLight(0, y, z);
                if (neighborLight > 1 &&
                    neighborLight - 1 > chunk.getSkyLight(CHUNK_SIZE_X - 1, y, z)) {
                    BlockIds id =
                        static_cast<BlockIds>(chunk.blockPosition[CHUNK_SIZE_X - 1][y][z]);
                    if (id == ID_AIR || isBlockTranslucent(id)) {
                        chunk.setSkyLight(CHUNK_SIZE_X - 1, y, z, neighborLight - 1);
                        lightQueue.push({CHUNK_SIZE_X - 1, y, z, coord.x, coord.z,
                                         (uint8_t)(neighborLight - 1)});
                    }
                }
            }
        }
    }

    if (ChunkHelper::activeChunks.count(neighbors[2])) {
        auto& frontChunk = ChunkHelper::activeChunks.at(neighbors[2]);
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                int neighborLight = frontChunk->getSkyLight(x, y, CHUNK_SIZE_Z - 1);
                if (neighborLight > 1 && neighborLight - 1 > chunk.getSkyLight(x, y, 0)) {
                    BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][0]);
                    if (id == ID_AIR || isBlockTranslucent(id)) {
                        chunk.setSkyLight(x, y, 0, neighborLight - 1);
                        lightQueue.push({x, y, 0, coord.x, coord.z, (uint8_t)(neighborLight - 1)});
                    }
                }
            }
        }
    }

    if (ChunkHelper::activeChunks.count(neighbors[3])) {
        auto& backChunk = ChunkHelper::activeChunks.at(neighbors[3]);
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                int neighborLight = backChunk->getSkyLight(x, y, 0);
                if (neighborLight > 1 &&
                    neighborLight - 1 > chunk.getSkyLight(x, y, CHUNK_SIZE_Z - 1)) {
                    BlockIds id =
                        static_cast<BlockIds>(chunk.blockPosition[x][y][CHUNK_SIZE_Z - 1]);
                    if (id == ID_AIR || isBlockTranslucent(id)) {
                        chunk.setSkyLight(x, y, CHUNK_SIZE_Z - 1, neighborLight - 1);
                        lightQueue.push({x, y, CHUNK_SIZE_Z - 1, coord.x, coord.z,
                                         (uint8_t)(neighborLight - 1)});
                    }
                }
            }
        }
    }

    propagateLightQueue(chunk, lightQueue);
}

static constexpr int dx[6] = {0, 0, -1, 1, 0, 0};
static constexpr int dy[6] = {0, 0, 0, 0, 1, -1};
static constexpr int dz[6] = {-1, 1, 0, 0, 0, 0};

void LightingSystem::spreadLightFromNeighbor(Chunk& chunk, Chunk& neighbor, int edge) {
#ifndef NDEBUG
    printf("DEBUG: spreadLightFromNeighbor - chunk(%d,%d) edge=%d\n", chunk.chunkCoords.x,
           chunk.chunkCoords.z, edge);
#endif

    std::queue<LightNode> lightQueue;
    int edgeUpdates = 0;

    switch (edge) {
        case 0:
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    int neighborLight = neighbor.getSkyLight(CHUNK_SIZE_X - 1, y, z);
                    BlockIds id = static_cast<BlockIds>(chunk.blockPosition[0][y][z]);
                    if ((id == ID_AIR || isBlockTranslucent(id)) && neighborLight > 1) {
                        uint8_t newLight = neighborLight - 1;
                        if (newLight > chunk.getSkyLight(0, y, z)) {
                            chunk.setSkyLight(0, y, z, newLight);
                            lightQueue.push({0, y, z, 0, 0, newLight});
                            edgeUpdates++;
                        }
                    }
                }
            }
            break;
    }

#ifndef NDEBUG
    printf("DEBUG: Edge updates: %d, queue size: %zu\n", edgeUpdates, lightQueue.size());
#endif

    int propagated = 0;
    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        if (node.lightLevel <= 1) continue;

        for (int f = 0; f < 6; f++) {
            int nx = node.x + dx[f];
            int ny = node.y + dy[f];
            int nz = node.z + dz[f];

            if (nx < 0 || nx >= CHUNK_SIZE_X || ny < 0 || ny >= CHUNK_SIZE_Y || nz < 0 ||
                nz >= CHUNK_SIZE_Z)
                continue;

            BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);
            if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

            uint8_t newLight = node.lightLevel - 1;
            if (newLight > chunk.getSkyLight(nx, ny, nz)) {
                chunk.setSkyLight(nx, ny, nz, newLight);
                lightQueue.push({nx, ny, nz, 0, 0, newLight});
                propagated++;
            }
        }
    }

#ifndef NDEBUG
    printf("DEBUG: Total propagated: %d\n", propagated);
#endif
}

int LightingSystem::getWorldLightLevel(int worldX, int worldY, int worldZ) {
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

void LightingSystem::propagateLightQueue(Chunk& chunk, std::queue<LightNode>& lightQueue) {
    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        if (node.lightLevel <= 1) continue;

        for (int f = 0; f < 6; f++) {
            int nx = node.x + dx1[f];
            int ny = node.y + dy1[f];
            int nz = node.z + dz1[f];

            if (nx < 0 || nx >= CHUNK_SIZE_X || ny < 0 || ny >= CHUNK_SIZE_Y || nz < 0 ||
                nz >= CHUNK_SIZE_Z)
                continue;

            BlockIds neighborId = static_cast<BlockIds>(chunk.blockPosition[nx][ny][nz]);
            if (neighborId != ID_AIR && !isBlockTranslucent(neighborId)) continue;

            uint8_t newLight = node.lightLevel - 1;
            if (newLight > chunk.getSkyLight(nx, ny, nz)) {
                chunk.setSkyLight(nx, ny, nz, newLight);
                lightQueue.push({nx, ny, nz, node.chunkX, node.chunkZ, newLight});
            }
        }
    }
}
