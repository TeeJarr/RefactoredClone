//
// Created by Tristan on 1/30/26.
//

#include "Chunk.hpp"

#include "Engine/Lighitng/LightingSystem.hpp"
#include <algorithm>

#include "FastNoiseLite.h"

#include "Block/Blocks.hpp"
#include "Engine/Settings.hpp"
#include <raymath.h>

constexpr int WATER_LEVEL = 62;
constexpr int BEACH_LEVEL = 63;

void ChunkHelper::generateChunkTerrain(const std::unique_ptr<Chunk>& chunk) {
    int chunkOffsetX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int chunkOffsetZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = chunkOffsetX + x;
            int wz = chunkOffsetZ + z;
            chunk->biomeMap[x][z] = ChunkHelper::getBiomeAt(wx, wz);

            float surfaceY = (getSurfaceHeight(wx, wz));
            chunk->surfaceHeight[x][z] = surfaceY;
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y == 0) {
                    chunk->blockPosition[x][y][z] = ID_BEDROCK;
                } else if (y <= surfaceY) {
                    chunk->blockPosition[x][y][z] = ID_STONE;
                } else {
                    chunk->blockPosition[x][y][z] = ID_AIR;
                }
            }

            if (surfaceY < WATER_LEVEL) {
                chunk->blockPosition[x][(int)surfaceY][z] = ID_WATER;
            }
        }
    }
}

bool ChunkHelper::isSolidBlock(int x, int y, int z) {
    float scale = 0.05f;
    float density = terrainNoise.GetNoise(x * scale, y * scale, z * scale);
    density = (density + 1.0f) * 0.5f;
    return density > 0.5f;
}

bool ChunkHelper::shouldPlaceTree(int wx, int wz) {
    BiomeType biomeType = getBiomeAt(wx, wz);
    const BiomeHelper& biome = biomes.at(biomeType);

    // No trees in this biome
    if (biome.treeChance <= 0.0f) return false;

    uint32_t px = static_cast<uint32_t>(wx + 1000000);
    uint32_t pz = static_cast<uint32_t>(wz + 1000000);

    uint32_t hash = px * 374761393u + pz * 668265263u + Settings::worldSeed;
    hash = (hash ^ (hash >> 13)) * 1274126177u;
    hash = hash ^ (hash >> 16);

    // Convert tree chance to threshold
    // treeChance 0.04 = 1 in 25, treeChance 0.003 = 1 in 333
    int threshold = static_cast<int>(1.0f / biome.treeChance);

    return (hash % threshold) == 0;
}

BiomeType ChunkHelper::getBiome(int wx, int wz) {
    float val =
        (biomeNoise.GetNoise(wx * 0.001f, wz * 0.001f) + 1.0f) * 0.5f; // low-frequency biome noise
    if (val < 0.4f) return BIOME_PLAINS;
    if (val < 0.7f) return BIOME_HILLS;
    return BIOME_MOUNTAINS;
}

float ChunkHelper::getChunkHeight(const Vector3& worldPosition) {
    float scale = 0.f;
    float height = ChunkHelper::terrainNoise.GetNoise(
        worldPosition.x * scale, worldPosition.y * scale, worldPosition.z * scale);

    height = (height + 1.0f) * 0.5f;

    return height * 64.0f + 64.0f;
}

bool ChunkHelper::isCave(int worldX, int y, int worldZ) {
    float frequency = 0.08f; // higher frequency = smaller caves
    float threshold = 0.65f;

    float value = caveNoise.GetNoise(worldX * frequency, y * frequency, worldZ * frequency);
    return value > threshold;
}

inline int floorDiv(int a, int b) {
    int r = a / b;
    int m = a % b;
    if ((m != 0) && ((m < 0) != (b < 0))) {
        r--;
    }
    return r;
}

inline int floorMod(int a, int b) {
    int m = a % b;
    if (m < 0) m += b;
    return m;
}

Chunk* ChunkHelper::getChunkFromWorld(int wx, int wz) {
    int cx = floorDiv(wx, CHUNK_SIZE_X);
    int cz = floorDiv(wz, CHUNK_SIZE_Z);

    ChunkCoord coord{cx, cz};

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end()) return nullptr;

    return it->second.get();
}

int ChunkHelper::getBlock(int wx, int wy, int wz) {
    auto chunk = getChunkFromWorld(wx, wz);
    if (!chunk) return ID_AIR;

    int lx = wx - chunk->chunkCoords.x * CHUNK_SIZE_X;
    int lz = wz - chunk->chunkCoords.z * CHUNK_SIZE_Z;

    if (wy < 0 || wy >= CHUNK_SIZE_Y) return ID_AIR;

    return chunk->blockPosition[lx][wy][lz];
}

void ChunkHelper::setBlock(int wx, int wy, int wz, int id) {
    Chunk* chunk = getChunkFromWorld(wx, wz);
    if (!chunk) return;

    int lx = floorMod(wx, CHUNK_SIZE_X);
    int lz = floorMod(wz, CHUNK_SIZE_Z);

    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;

    chunk->blockPosition[lx][wy][lz] = id;
    chunk->dirty = true;
}

ChunkCoord ChunkHelper::worldToChunkCoord(int wx, int wz) {
    auto conv = [](int v, int size) { return (v >= 0) ? (v / size) : ((v - size + 1) / size); };

    return {conv(wx, CHUNK_SIZE_X), conv(wz, CHUNK_SIZE_Z)};
}

void ChunkHelper::markChunkDirty(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(activeChunksMutex);

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end()) return;

    Chunk* chunk = it->second.get();
    if (!chunk) return;
    if (chunk->dirty) return;               // Already dirty
    if (chunk->meshBuilding.load()) return; // Already rebuilding

    chunk->dirty = true;
}

int ChunkHelper::WorldToChunk(int w) {
    return (w >= 0) ? w / CHUNK_SIZE_X : (w - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
}

int ChunkHelper::WorldToLocal(int w) {
    int m = w % CHUNK_SIZE_X;
    return (m < 0) ? m + CHUNK_SIZE_X : m;
}

float ChunkHelper::getBiomeFactor(int wx, int wz) {
    // Low frequency noise to select biome
    float freq = 0.5f;                                           // large-scale features
    float val = biomeNoise.GetNoise(wx * freq, 0.0f, wz * freq); // -1..1
    val = (val + 1.0f) / 2.0f;                                   // 0..1
    return val;
}

BiomeHelper ChunkHelper::getBiomeType(float biomeFactor) {
    if (biomeFactor < 0.6) {
        return biomes.at(BIOME_PLAINS);
    } else if (biomeFactor < 0.85) {
        return biomes.at(BIOME_HILLS);
    } else {
        return biomes.at(BIOME_MOUNTAINS);
    }
}

std::unique_ptr<Chunk> ChunkHelper::generateChunkAsync(const Vector3& chunkPosIndex) {
    auto chunk = std::make_unique<Chunk>();

    int chunkX = static_cast<int>(chunkPosIndex.x);
    int chunkZ = static_cast<int>(chunkPosIndex.z);
    chunk->chunkCoords.x = chunkX;
    chunk->chunkCoords.z = chunkZ;

    generateChunkTerrain(chunk); // terrain + caves
    setBiomeFloor(chunk);        // grass/dirt/sand
    populateTrees(*chunk);       // trees

    chunk->boundingBox.min = {(float)chunkX * CHUNK_SIZE_X, 0.0f, (float)chunkZ * CHUNK_SIZE_Z};
    chunk->boundingBox.max = {(float)chunkX * CHUNK_SIZE_X + CHUNK_SIZE_X, (float)CHUNK_SIZE_Y,
                              (float)chunkZ * CHUNK_SIZE_Z + CHUNK_SIZE_Z};

    chunk->chunkId = (chunkX & 0xFFFF) | ((chunkZ & 0xFFFF) << 16);
    chunk->loaded = false;
    chunk->alpha = 0.0f;

    return chunk;
}

// Add to ChunkHelper
BlockIds ChunkHelper::getWorldBlock(int worldX, int worldY, int worldZ) {
    if (worldY < 0 || worldY >= CHUNK_SIZE_Y) {
        return ID_AIR;
    }

    ChunkCoord coord = worldToChunkCoord(worldX, worldZ);

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end() || !it->second) {
        return ID_AIR; // Assume air if chunk not loaded
    }

    int localX = WorldToLocal(worldX);
    int localZ = WorldToLocal(worldZ);

    return static_cast<BlockIds>(it->second->blockPosition[localX][worldY][localZ]);
}

void ChunkHelper::setBiomeFloor(const std::unique_ptr<Chunk>& chunk) {
    int baseX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int baseZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = baseX + x;
            int wz = baseZ + z;
            int surfaceY = static_cast<int>(chunk->surfaceHeight[x][z]);

            BiomeType biomeType = getBiomeAt(wx, wz);
            const BiomeHelper& biome = biomes.at(biomeType);

            // Underwater
            if (surfaceY < WATER_LEVEL) {
                // Fill water above surface
                for (int y = surfaceY + 1; y <= WATER_LEVEL; y++) {
                    chunk->blockPosition[x][y][z] = ID_WATER;
                }
                // Ocean/river floor is sand
                if (chunk->blockPosition[x][surfaceY][z] == ID_STONE) {
                    chunk->blockPosition[x][surfaceY][z] = ID_SAND;
                }
                continue;
            }

            // Surface block
            if (chunk->blockPosition[x][surfaceY][z] == ID_STONE) {
                chunk->blockPosition[x][surfaceY][z] = biome.surfaceBlock;
            }

            // Subsurface (3 blocks deep)
            for (int y = surfaceY - 1; y >= surfaceY - 3 && y > 0; y--) {
                if (chunk->blockPosition[x][y][z] == ID_STONE) {
                    chunk->blockPosition[x][y][z] = biome.subsurfaceBlock;
                }
            }

            // Snow on mountain tops
            if (biomeType == BIOME_MOUNTAINS && surfaceY > 110) {
                // TODO: Add snow
                chunk->blockPosition[x][surfaceY][z] = ID_GRASS;
            }
        }
    }
}

// ---- Tree Placement ----
void ChunkHelper::populateTrees(Chunk& chunk) {
    int baseX = chunk.chunkCoords.x * CHUNK_SIZE_X;
    int baseZ = chunk.chunkCoords.z * CHUNK_SIZE_Z;

    for (int lx = 2; lx < CHUNK_SIZE_X - 2; lx++) {
        // Avoid edges
        for (int lz = 2; lz < CHUNK_SIZE_Z - 2; lz++) {
            int wx = baseX + lx;
            int wz = baseZ + lz;

            int surfaceY = static_cast<int>(chunk.surfaceHeight[lx][lz]);

            if (surfaceY <= WATER_LEVEL) continue;
            if (surfaceY >= CHUNK_SIZE_Y - 15) continue;
            if (chunk.blockPosition[lx][surfaceY][lz] != ID_GRASS) continue;
            if (!shouldPlaceTree(wx, wz)) continue;

            // Generate tree inline
            uint32_t h = wx * 928371u ^ wz * 123721u ^ Settings::worldSeed;
            int height = 4 + (h & 3);

            // Trunk
            for (int y = 1; y <= height; y++) {
                chunk.blockPosition[lx][surfaceY + y][lz] = ID_OAK_WOOD;
            }

            // Leaves
            int topY = surfaceY + height;
            for (int dx = -2; dx <= 2; dx++) {
                for (int dz = -2; dz <= 2; dz++) {
                    for (int dy = 0; dy <= 2; dy++) {
                        int dist = abs(dx) + abs(dz) + abs(dy);
                        if (dist > 3) continue;

                        int nx = lx + dx;
                        int ny = topY + dy;
                        int nz = lz + dz;

                        if (nx >= 0 && nx < CHUNK_SIZE_X && ny < CHUNK_SIZE_Y && nz >= 0 &&
                            nz < CHUNK_SIZE_Z) {
                            if (chunk.blockPosition[nx][ny][nz] == ID_AIR) {
                                chunk.blockPosition[nx][ny][nz] = ID_OAK_LEAF;
                            }
                        }
                    }
                }
            }
        }
    }
}

// ---- Tree Generation ----
void ChunkHelper::generateTree(int baseWorldX, int baseWorldY, int baseWorldZ) {
    uint32_t h = baseWorldX * 928371u ^ baseWorldZ * 123721u ^ Settings::worldSeed;
    int height = 4 + (h & 3); // 4–7 blocks tall

    for (int y = 0; y < height; y++)
        ChunkHelper::setBlock(baseWorldX, baseWorldY + y, baseWorldZ, ID_OAK_WOOD);

    int topY = baseWorldY + height;
    for (int dx = -2; dx <= 2; dx++) {
        for (int dz = -2; dz <= 2; dz++) {
            for (int dy = -1; dy <= 2; dy++) {
                int dist = abs(dx) + abs(dz) + abs(dy);
                if (dist > 4) continue;
                ChunkHelper::setBlock(baseWorldX + dx, topY + dy, baseWorldZ + dz, ID_OAK_LEAF);
            }
        }
    }
}

float ChunkHelper::LERP(float a, float b, float t) { return a + t * (b - a); }

// Biome selection functions

float ChunkHelper::getTemperature(int wx, int wz) {
    float temp = temperatureNoise.GetNoise((float)wx, (float)wz);
    return (temp + 1.0f) * 0.5f;
}

float ChunkHelper::getHumidity(int wx, int wz) {
    float humid = humidityNoise.GetNoise((float)wx, (float)wz);
    return (humid + 1.0f) * 0.5f;
}

float ChunkHelper::getContinentalness(int wx, int wz) {
    float cont = continentalnessNoise.GetNoise((float)wx, (float)wz);
    return (cont + 1.0f) * 0.5f;
}

void ChunkHelper::initNoiseRenderer() {
    // ---------------------------
    // Base terrain
    // ---------------------------
    terrainNoise.SetSeed(Settings::worldSeed);
    terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise.SetFractalOctaves(5);
    terrainNoise.SetFractalLacunarity(2.0f);
    terrainNoise.SetFractalGain(0.5f);
    terrainNoise.SetFrequency(0.005f);

    // ---------------------------
    // Continentalness (large scale land/ocean)
    // ---------------------------
    continentalnessNoise.SetSeed(Settings::worldSeed + 1000);
    continentalnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    continentalnessNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentalnessNoise.SetFractalOctaves(3);
    continentalnessNoise.SetFrequency(0.0005f); // Very low = large continents

    // ---------------------------
    // Erosion
    // ---------------------------
    erosionNoise.SetSeed(Settings::worldSeed + 2000);
    erosionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    erosionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    erosionNoise.SetFractalOctaves(4);
    erosionNoise.SetFractalGain(0.4f);
    erosionNoise.SetFrequency(0.003f);

    // ---------------------------
    // Peaks (hills/mountains regions)
    // ---------------------------
    peakNoise.SetSeed(Settings::worldSeed + 3000);
    peakNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    peakNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    peakNoise.SetFractalOctaves(4);
    peakNoise.SetFrequency(0.001f); // Low = large mountain ranges

    // ---------------------------
    // Detail
    // ---------------------------
    detailNoise.SetSeed(Settings::worldSeed + 4000);
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(3);
    detailNoise.SetFrequency(0.02f);

    // ---------------------------
    // Temperature (LARGE biomes)
    // ---------------------------
    temperatureNoise.SetSeed(Settings::worldSeed + 5000);
    temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    temperatureNoise.SetFractalOctaves(3);
    temperatureNoise.SetFrequency(0.0009f); // Very low = large temperature zones

    // ---------------------------
    // Humidity (LARGE biomes)
    // ---------------------------
    humidityNoise.SetSeed(Settings::worldSeed + 6000);
    humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    humidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    humidityNoise.SetFractalOctaves(3);
    humidityNoise.SetFrequency(0.0009f); // Very low = large humidity zones

    // ---------------------------
    // Biome
    // ---------------------------
    biomeNoise.SetSeed(Settings::worldSeed + 7000);
    biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    biomeNoise.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Hybrid);
    biomeNoise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
    biomeNoise.SetFrequency(0.008f); // Lower = larger biome cells

    // ---------------------------
    // Caves
    // ---------------------------
    caveNoise.SetSeed(Settings::worldSeed + 8000);
    caveNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    caveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise.SetFractalOctaves(2);
    caveNoise.SetFrequency(0.05f);

    // ---------------------------
    // Trees
    // ---------------------------
    treeNoise.SetSeed(Settings::worldSeed + 9000);
    treeNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    treeNoise.SetFrequency(0.5f);
}

float ChunkHelper::getSurfaceHeight(int wx, int wz) {
    float x = (float)wx;
    float z = (float)wz;

    const float SEA_LEVEL = 62.0f;
    const float BASE_HEIGHT = 64.0f;

    float continental = continentalnessNoise.GetNoise(x, z);
    float terrain = terrainNoise.GetNoise(x, z);
    float erosion = erosionNoise.GetNoise(x, z);
    float peaks = peakNoise.GetNoise(x, z);
    float detail = detailNoise.GetNoise(x, z);

    float localVar = terrainNoise.GetNoise(x * 2.0f, z * 2.0f);
    float microVar = detailNoise.GetNoise(x * 1.5f, z * 1.5f);

    BiomeType biome = getBiomeAt(wx, wz);

    float height;

    const float OCEAN_THRESHOLD = -0.3f;
    const float BEACH_WIDTH = 0.03f;

    // Deep Ocean
    if (continental < OCEAN_THRESHOLD - 0.2f) {
        height = SEA_LEVEL - 20.0f;
        height += terrain * 5.0f;
        return std::max(height, 5.0f);
    }

    // Shallow Ocean
    if (continental < OCEAN_THRESHOLD - BEACH_WIDTH) {
        float depth = (OCEAN_THRESHOLD - BEACH_WIDTH - continental) / 0.2f;
        height = SEA_LEVEL - 3.0f - depth * 15.0f;
        height += terrain * 3.0f;
        return std::max(height, 5.0f);
    }

    // Beach
    if (continental < OCEAN_THRESHOLD + BEACH_WIDTH) {
        float t = (continental - (OCEAN_THRESHOLD - BEACH_WIDTH)) / (BEACH_WIDTH * 2.0f);
        height = SEA_LEVEL - 2.0f + t * 5.0f;
        return height;
    }

    // --- Land ---
    height = BASE_HEIGHT;

    float flatness = (erosion + 1.0f) * 0.5f;
    flatness = 0.3f + flatness * 0.7f;

    // Base terrain for all land biomes
    height += terrain * 8.0f * flatness;
    height += localVar * 5.0f * flatness;
    height += microVar * 2.0f;
    height += detail * 1.5f;

    // Gradual mountain/hill influence based on peaks value
    // This creates smooth transitions instead of hard boundaries
    if (peaks > -0.2f) {
        // Gradually increase height as peaks increases
        // -0.2 to 0.0 = slight hills
        // 0.0 to 0.2 = medium hills
        // 0.2 to 0.4 = large hills
        // 0.4+ = mountains

        float hillFactor = (peaks + 0.2f) / 0.4f;  // 0 to 1 for hills
        hillFactor = std::clamp(hillFactor, 0.0f, 1.0f);
        hillFactor = hillFactor * hillFactor;  // Smooth curve

        height += hillFactor * 15.0f;

        // Additional mountain height
        if (peaks > 0.1f) {
            float mountainFactor = (peaks - 0.1f) / 0.3f;  // 0 to 1
            mountainFactor = std::clamp(mountainFactor, 0.0f, 1.0f);
            mountainFactor = mountainFactor * mountainFactor;  // Smooth curve

            height += mountainFactor * 25.0f;
        }

        // Extreme peaks
        if (peaks > 0.3f) {
            float extremeFactor = (peaks - 0.3f) / 0.4f;
            extremeFactor = std::clamp(extremeFactor, 0.0f, 1.0f);
            extremeFactor = extremeFactor * extremeFactor;

            height += extremeFactor * 35.0f;
        }
    }

    // Biome-specific adjustments (on top of gradual terrain)
    switch (biome) {
        case BIOME_DESERT:
            // Deserts are slightly elevated and always above water
            height += 2.0f;
            height = std::max(height, SEA_LEVEL + 3.0f);
            break;

        case BIOME_SWAMP:
            // Swamps are flattened toward sea level
            height = SEA_LEVEL + 1.0f + (height - BASE_HEIGHT) * 0.2f;
            break;

        case BIOME_TAIGA:
            // Slightly elevated cold terrain
            height += 2.0f;
            break;

        default:
            break;
    }

    return std::clamp(height, 1.0f, (float)(CHUNK_SIZE_Y - 5));
}

BiomeType ChunkHelper::getBiomeAt(int wx, int wz) {
    float x = (float)wx;
    float z = (float)wz;

    float continental = continentalnessNoise.GetNoise(x, z);
    float temperature = temperatureNoise.GetNoise(x, z);
    float humidity = humidityNoise.GetNoise(x, z);
    float peaks = peakNoise.GetNoise(x, z);

    const float OCEAN_THRESHOLD = -0.3f;
    const float BEACH_WIDTH = 0.03f;

    // Ocean
    if (continental < OCEAN_THRESHOLD - BEACH_WIDTH) {
        return BIOME_OCEAN;
    }

    // Beach
    if (continental < OCEAN_THRESHOLD + BEACH_WIDTH) {
        if (temperature > 0.15f && humidity < -0.1f) {
            return BIOME_DESERT;
        }
        return BIOME_BEACH;
    }

    // Mountains
    if (peaks > 0.35f) {
        return BIOME_MOUNTAINS;
    }

    // Hills
    if (peaks > 0.15f) {
        // if (temperature < -0.8f) {
        //     return BIOME_TAIGA;
        // }
        return BIOME_HILLS;
    }

    // Desert
    if (temperature > 0.15f && humidity < -0.1f) {
        return BIOME_DESERT;
    }

    // Swamp
    if (temperature > -0.1f && humidity > 0.25f && peaks < 0.0f) {
        return BIOME_SWAMP;
    }

    // // Taiga - only in VERY cold areas
    if (temperature < -0.8f) {
        return BIOME_TAIGA;
    }

    // Forest
    if (humidity > 0.05f) {
        return BIOME_FOREST;
    }

    return BIOME_PLAINS;
}