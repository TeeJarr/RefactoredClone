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

// void ChunkHelper::generateChunkTerrain(const std::unique_ptr<Chunk>& chunk) {
//     int chunkOffsetX = chunk->chunkCoords.x * CHUNK_SIZE_X;
//     int chunkOffsetZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;
//
//     for (int x = 0; x < CHUNK_SIZE_X; x++) {
//         for (int z = 0; z < CHUNK_SIZE_Z; z++) {
//             int wx = chunkOffsetX + x;
//             int wz = chunkOffsetZ + z;
//             chunk->biomeMap[x][z] = ChunkHelper::getBiomeAt(wx, wz);
//
//             float surfaceY = (getSurfaceHeight(wx, wz));
//             chunk->surfaceHeight[x][z] = surfaceY;
//             for (int y = 0; y < CHUNK_SIZE_Y; y++) {
//                 if (y == 0) {
//                     chunk->blockPosition[x][y][z] = ID_BEDROCK;
//                 } else if (y <= surfaceY) {
//                     chunk->blockPosition[x][y][z] = ID_STONE;
//                 } else {
//                     chunk->blockPosition[x][y][z] = ID_AIR;
//                 }
//             }
//
//             if (surfaceY < WATER_LEVEL) {
//                 chunk->blockPosition[x][(int)surfaceY][z] = ID_WATER;
//             }
//         }
//     }
// }

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
    // setBiomeFloor(chunk);        // grass/dirt/sand
    populateTrees(*chunk); // trees

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

            int subSurface = GetRandomValue(4, 7);
            for (int y = surfaceY - 1; y >= surfaceY - subSurface && y > 0; y--) {
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
    // Region noise (VERY large scale - continents/major climate zones)
    // ---------------------------
    regionNoise.SetSeed(Settings::worldSeed);
    regionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    regionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    regionNoise.SetFractalOctaves(2);
    regionNoise.SetFrequency(0.00005f); // Huge regions

    // ---------------------------
    // Sub-region noise (large scale - biome clusters)
    // ---------------------------
    subRegionNoise.SetSeed(Settings::worldSeed + 100);
    subRegionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    subRegionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    subRegionNoise.SetFractalOctaves(3);
    subRegionNoise.SetFrequency(0.0002f); // Large regions

    // ---------------------------
    // Local noise (medium scale - individual biomes)
    // ---------------------------
    localNoise.SetSeed(Settings::worldSeed + 200);
    localNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    localNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    localNoise.SetFractalOctaves(3);
    localNoise.SetFrequency(0.001f); // Medium regions

    // ---------------------------
    // Weirdness (for unusual terrain)
    // ---------------------------
    weirdnessNoise.SetSeed(Settings::worldSeed + 300);
    weirdnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    weirdnessNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    weirdnessNoise.SetFractalOctaves(2);
    weirdnessNoise.SetFrequency(0.0004f);

    // ---------------------------
    // Temperature (multi-scale)
    // ---------------------------
    temperatureNoise.SetSeed(Settings::worldSeed + 1000);
    temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    temperatureNoise.SetFractalOctaves(2);
    temperatureNoise.SetFrequency(0.00008f); // Very large temperature zones

    // ---------------------------
    // Humidity (multi-scale)
    // ---------------------------
    humidityNoise.SetSeed(Settings::worldSeed + 2000);
    humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    humidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    humidityNoise.SetFractalOctaves(2);
    humidityNoise.SetFrequency(0.00008f); // Very large humidity zones

    // ---------------------------
    // Continentalness (multi-scale)
    // ---------------------------
    continentalnessNoise.SetSeed(Settings::worldSeed + 3000);
    continentalnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    continentalnessNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentalnessNoise.SetFractalOctaves(3);
    continentalnessNoise.SetFrequency(0.0001f); // Large continents

    // ---------------------------
    // Erosion
    // ---------------------------
    erosionNoise.SetSeed(Settings::worldSeed + 4000);
    erosionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    erosionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    erosionNoise.SetFractalOctaves(3);
    erosionNoise.SetFrequency(0.0006f);

    // ---------------------------
    // Peaks/Valleys
    // ---------------------------
    peakNoise.SetSeed(Settings::worldSeed + 5000);
    peakNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    peakNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    peakNoise.SetFractalOctaves(4);
    peakNoise.SetFrequency(0.0008f);

    // ---------------------------
    // Base terrain
    // ---------------------------
    terrainNoise.SetSeed(Settings::worldSeed + 6000);
    terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise.SetFractalOctaves(5);
    terrainNoise.SetFractalLacunarity(2.0f);
    terrainNoise.SetFractalGain(0.5f);
    terrainNoise.SetFrequency(0.003f);

    // ---------------------------
    // Detail
    // ---------------------------
    detailNoise.SetSeed(Settings::worldSeed + 7000);
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(3);
    detailNoise.SetFrequency(0.015f);

    // ---------------------------
    // Caves
    // ---------------------------
    caveNoise.SetSeed(Settings::worldSeed + 8000);
    caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    caveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise.SetFractalOctaves(2);
    caveNoise.SetFrequency(0.015f);

    caveNoise2.SetSeed(Settings::worldSeed + 9000);
    caveNoise2.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    caveNoise2.SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise2.SetFractalOctaves(2);
    caveNoise2.SetFrequency(0.02f);

    cheeseCaveNoise.SetSeed(Settings::worldSeed + 10000);
    cheeseCaveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    cheeseCaveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    cheeseCaveNoise.SetFractalOctaves(2);
    cheeseCaveNoise.SetFrequency(0.01f);

    overhangNoise.SetSeed(Settings::worldSeed + 11000);
    overhangNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    overhangNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    overhangNoise.SetFractalOctaves(3);
    overhangNoise.SetFrequency(0.02f);

    treeNoise.SetSeed(Settings::worldSeed + 12000);
    treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    treeNoise.SetFrequency(0.5f);
}

float ChunkHelper::getSurfaceHeight(int wx, int wz) {
    float x = (float)wx;
    float z = (float)wz;

    const float SEA_LEVEL = 62.0f;
    const float BASE_HEIGHT = 64.0f;

    ClimateData climate = getClimateAt(wx, wz);
    BiomeType biome = getBiomeAt(wx, wz);

    // Multi-scale terrain
    float terrain = terrainNoise.GetNoise(x, z);
    float terrain2 = terrainNoise.GetNoise(x * 2.0f, z * 2.0f) * 0.4f;
    float terrain3 = terrainNoise.GetNoise(x * 4.0f, z * 4.0f) * 0.2f;
    float detail = detailNoise.GetNoise(x, z);

    float height;

    const float OCEAN_THRESHOLD = -0.35f;
    const float BEACH_WIDTH = 0.03f;

    // === OCEAN ===
    if (climate.continental < OCEAN_THRESHOLD - 0.15f) {
        // Deep ocean
        float oceanDepth = (OCEAN_THRESHOLD - 0.15f - climate.continental) / 0.5f;
        oceanDepth = std::clamp(oceanDepth, 0.0f, 1.0f);
        height = SEA_LEVEL - 8.0f - oceanDepth * 12.0f;
        height += terrain * 3.0f;
        return std::max(height, 40.0f);
    }

    if (climate.continental < OCEAN_THRESHOLD - BEACH_WIDTH) {
        // Shallow ocean
        float shallowFactor =
            (climate.continental - (OCEAN_THRESHOLD - 0.15f)) / (0.15f - BEACH_WIDTH);
        shallowFactor = std::clamp(shallowFactor, 0.0f, 1.0f);
        height = SEA_LEVEL - 8.0f + shallowFactor * 6.0f;
        height += terrain * 2.0f;
        return std::max(height, 50.0f);
    }

    // === BEACH ===
    if (climate.continental < OCEAN_THRESHOLD + BEACH_WIDTH) {
        float beachFactor =
            (climate.continental - (OCEAN_THRESHOLD - BEACH_WIDTH)) / (BEACH_WIDTH * 2.0f);
        beachFactor = std::clamp(beachFactor, 0.0f, 1.0f);
        height = SEA_LEVEL - 1.0f + beachFactor * 4.0f;
        height += detail * 0.5f;
        return height;
    }

    // === LAND ===
    height = BASE_HEIGHT;

    // Coastal factor for smooth transitions
    float distanceFromCoast = climate.continental - OCEAN_THRESHOLD;
    float coastalFactor = std::clamp(distanceFromCoast / 0.25f, 0.0f, 1.0f);
    coastalFactor = coastalFactor * coastalFactor;

    // Erosion controls flatness
    float flatness = (climate.erosion + 1.0f) * 0.5f;
    flatness = 0.4f + flatness * 0.6f;

    // Base terrain (reduced near coast)
    height += terrain * 5.0f * flatness * (0.6f + 0.4f * coastalFactor);
    height += terrain2 * 3.0f * flatness * coastalFactor;
    height += terrain3 * 1.5f * coastalFactor;
    height += detail * 1.5f;

    // Small hills
    if (climate.peaks > -0.2f) {
        float hillFactor = (climate.peaks + 0.2f) / 0.4f;
        hillFactor = std::clamp(hillFactor, 0.0f, 1.0f);
        hillFactor = hillFactor * hillFactor;
        height += hillFactor * 6.0f * coastalFactor;
    }

    // Medium hills (inland only)
    if (climate.peaks > 0.15f && coastalFactor > 0.5f) {
        float mediumFactor = (climate.peaks - 0.15f) / 0.25f;
        mediumFactor = std::clamp(mediumFactor, 0.0f, 1.0f);
        mediumFactor = mediumFactor * mediumFactor;
        height += mediumFactor * 10.0f * coastalFactor;
    }

    // Mountains (rare, far inland)
    if (climate.peaks > 0.4f && coastalFactor > 0.8f) {
        float mountainFactor = (climate.peaks - 0.4f) / 0.4f;
        mountainFactor = std::clamp(mountainFactor, 0.0f, 1.0f);
        mountainFactor = mountainFactor * mountainFactor;
        height += mountainFactor * 18.0f;
    }

    // Biome-specific adjustments
    switch (biome) {
        case BIOME_DESERT:
            height = std::max(height, SEA_LEVEL + 2.0f);
            break;
        case BIOME_SWAMP:
            height = SEA_LEVEL + 1.0f + (height - BASE_HEIGHT) * 0.2f;
            break;
        case BIOME_MOUNTAINS:
            if (coastalFactor > 0.7f) {
                height += 4.0f;
            }
            break;
        default:
            break;
    }

    height = std::min(height, 95.0f);

    return std::clamp(height, 1.0f, (float)(CHUNK_SIZE_Y - 5));
}

BiomeType ChunkHelper::getBiomeAt(int wx, int wz) {
    ClimateData climate = getClimateAt(wx, wz);

    const float OCEAN_THRESHOLD = -0.35f;
    const float BEACH_WIDTH = 0.03f;

    float distanceFromCoast = climate.continental - OCEAN_THRESHOLD;
    float coastalFactor = std::clamp(distanceFromCoast / 0.25f, 0.0f, 1.0f);
    coastalFactor = coastalFactor * coastalFactor;

    // === OCEAN ===
    if (climate.continental < OCEAN_THRESHOLD - BEACH_WIDTH) {
        // Deep ocean vs shallow based on continental value
        return BIOME_OCEAN;
    }

    // === BEACH ===
    if (climate.continental < OCEAN_THRESHOLD + BEACH_WIDTH) {
        // Hot beaches can be desert coastline
        if (climate.temperature > 0.3f && climate.humidity < -0.2f) {
            return BIOME_DESERT;
        }
        return BIOME_BEACH;
    }

    // === MOUNTAINS (rare, only far inland with high peaks) ===
    if (climate.peaks > 0.90f && coastalFactor > 0.7f && climate.erosion > -0.3f) {
        return BIOME_MOUNTAINS;
    }

    // === HILLS (uncommon, inland) ===
    if (climate.peaks > 0.75f && coastalFactor > 0.4f) {
        if (climate.temperature < -0.4f) {
            return BIOME_TAIGA;
        }
        return BIOME_HILLS;
    }

    // === CLIMATE-BASED BIOMES ===

    // Hot & Dry = Desert
    if (climate.temperature > 0.3f && climate.humidity < -0.15f) {
        return BIOME_DESERT;
    }

    // Hot & Wet & Flat = Swamp
    if (climate.temperature > 0.0f && climate.humidity > 0.3f && climate.peaks < -0.1f) {
        return BIOME_SWAMP;
    }

    // Cold = Taiga
    if (climate.temperature < -0.4f && coastalFactor > 0.3f) {
        return BIOME_TAIGA;
    }

    // Humid = Forest
    if (climate.humidity > 0.05f) {
        return BIOME_FOREST;
    }

    // Default = Plains
    return BIOME_PLAINS;
}

void ChunkHelper::generateChunkTerrain(const std::unique_ptr<Chunk>& chunk) {
    int chunkOffsetX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int chunkOffsetZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    const int SEA_LEVEL = 62;
    const int STONE_DEPTH = 4;

    // First pass: generate base terrain
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = chunkOffsetX + x;
            int wz = chunkOffsetZ + z;

            BiomeType biome = ChunkHelper::getBiomeAt(wx, wz);
            chunk->biomeMap[x][z] = biome;

            int surfaceY = (int)getSurfaceHeight(wx, wz);
            chunk->surfaceHeight[x][z] = surfaceY;

            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockIds block = ID_AIR;

                if (y == 0) {
                    block = ID_BEDROCK;
                } else if (y < 5) {
                    if (GetRandomValue(0, y) == 0) {
                        block = ID_BEDROCK;
                    } else {
                        block = ID_STONE;
                    }
                } else if (y < surfaceY - STONE_DEPTH) {
                    block = ID_STONE;
                } else if (y < surfaceY) {
                    switch (biome) {
                        case BIOME_DESERT:
                        case BIOME_BEACH:
                            block = ID_SAND;
                            break;
                        case BIOME_MOUNTAINS:
                            if (surfaceY > 90) {
                                block = ID_STONE;
                            } else {
                                block = ID_DIRT;
                            }
                            break;
                        default:
                            block = ID_DIRT;
                            break;
                    }
                } else if (y == surfaceY) {
                    switch (biome) {
                        case BIOME_DESERT:
                        case BIOME_BEACH:
                        case BIOME_OCEAN:
                            block = ID_SAND;
                            break;
                        case BIOME_MOUNTAINS:
                            if (surfaceY > 90) {
                                block = ID_STONE;
                            } else if (surfaceY > 80) {
                                block = (GetRandomValue(0, 100) < 40) ? ID_GRASS : ID_GRASS;
                            } else {
                                block = ID_GRASS;
                            }
                            break;
                        default:
                            block = ID_GRASS;
                            break;
                    }
                } else if (y <= SEA_LEVEL && y > surfaceY) {
                    if (biome == BIOME_DESERT) {
                        block = ID_AIR;
                    } else {
                        block = ID_WATER;
                    }
                }

                chunk->blockPosition[x][y][z] = block;
            }
        }
    }

    // Second pass: caves and overhangs (LESS COMMON)
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = chunkOffsetX + x;
            int wz = chunkOffsetZ + z;
            int surfaceY = chunk->surfaceHeight[x][z];

            for (int y = 1; y < CHUNK_SIZE_Y - 1; y++) {
                float wy = (float)y;

                if (chunk->blockPosition[x][y][z] == ID_AIR ||
                    chunk->blockPosition[x][y][z] == ID_WATER) {
                    continue;
                }

                // --- OVERHANGS (rarer) ---
                if (y > 50 && y < surfaceY + 15) {
                    float overhang = overhangNoise.GetNoise((float)wx, wy, (float)wz);

                    float heightFactor = 1.0f - std::abs(y - 70.0f) / 40.0f;
                    heightFactor = std::max(0.0f, heightFactor);

                    float steepness = std::abs(peakNoise.GetNoise((float)wx, (float)wz));

                    // Higher threshold = rarer overhangs
                    float overhangThreshold = 0.55f - heightFactor * 0.1f - steepness * 0.05f;

                    if (overhang > overhangThreshold && y > surfaceY - 8) {
                        chunk->blockPosition[x][y][z] = ID_AIR;
                    }
                }

                // --- SPAGHETTI CAVES (much rarer) ---
                if (y > 8 && y < surfaceY - 8) {
                    float cave1 = caveNoise.GetNoise((float)wx, wy, (float)wz);
                    float cave2 = caveNoise2.GetNoise((float)wx, wy, (float)wz);

                    float spaghettiValue = std::abs(cave1) + std::abs(cave2);

                    float depthFactor = 1.0f - (float)y / (float)surfaceY;
                    depthFactor = std::clamp(depthFactor, 0.0f, 1.0f);

                    // Much lower threshold = much rarer caves
                    float caveThreshold = 0.08f + depthFactor * 0.04f;

                    if (spaghettiValue < caveThreshold) {
                        chunk->blockPosition[x][y][z] = ID_AIR;
                    }
                }

                // --- CHEESE CAVES (rarer and smaller) ---
                if (y > 15 && y < 45) {
                    float cheese = cheeseCaveNoise.GetNoise((float)wx, wy, (float)wz);

                    // Higher threshold = rarer cheese caves
                    float cheeseThreshold = 0.75f;

                    if (cheese > cheeseThreshold) {
                        chunk->blockPosition[x][y][z] = ID_AIR;
                    }
                }
            }
        }
    }

    // Third pass: cleanup
    for (int x = 1; x < CHUNK_SIZE_X - 1; x++) {
        for (int z = 1; z < CHUNK_SIZE_Z - 1; z++) {
            for (int y = 2; y < CHUNK_SIZE_Y - 1; y++) {
                if (chunk->blockPosition[x][y][z] == ID_DIRT ||
                    chunk->blockPosition[x][y][z] == ID_GRASS) {

                    bool exposedToAir = false;
                    if (y > 0 && chunk->blockPosition[x][y - 1][z] == ID_AIR) exposedToAir = true;
                    if (chunk->blockPosition[x + 1][y][z] == ID_AIR) exposedToAir = true;
                    if (chunk->blockPosition[x - 1][y][z] == ID_AIR) exposedToAir = true;
                    if (chunk->blockPosition[x][y][z + 1] == ID_AIR) exposedToAir = true;
                    if (chunk->blockPosition[x][y][z - 1] == ID_AIR) exposedToAir = true;

                    int surfaceY = chunk->surfaceHeight[x][z];
                    if (exposedToAir && y < surfaceY - 2) {
                        chunk->blockPosition[x][y][z] = ID_STONE;
                    }
                }

                // Remove floating blocks
                if (chunk->blockPosition[x][y][z] != ID_AIR &&
                    chunk->blockPosition[x][y][z] != ID_WATER &&
                    chunk->blockPosition[x][y][z] != ID_BEDROCK) {

                    int airCount = 0;
                    if (chunk->blockPosition[x][y - 1][z] == ID_AIR) airCount++;
                    if (chunk->blockPosition[x][y + 1][z] == ID_AIR) airCount++;
                    if (chunk->blockPosition[x + 1][y][z] == ID_AIR) airCount++;
                    if (chunk->blockPosition[x - 1][y][z] == ID_AIR) airCount++;
                    if (chunk->blockPosition[x][y][z + 1] == ID_AIR) airCount++;
                    if (chunk->blockPosition[x][y][z - 1] == ID_AIR) airCount++;

                    if (airCount >= 5) {
                        chunk->blockPosition[x][y][z] = ID_AIR;
                    }
                }
            }
        }
    }
}

ClimateData ChunkHelper::getClimateAt(int wx, int wz) {
    float x = (float)wx;
    float z = (float)wz;

    ClimateData climate;

    // === TEMPERATURE ===
    // Large scale (climate zones)
    float tempLarge = temperatureNoise.GetNoise(x, z);
    // Medium scale (regional variation)
    float tempMedium = temperatureNoise.GetNoise(x * 3.0f, z * 3.0f) * 0.3f;
    // Small scale (local variation)
    float tempSmall = localNoise.GetNoise(x * 0.5f + 5000.0f, z * 0.5f) * 0.1f;

    climate.temperature = tempLarge + tempMedium + tempSmall;
    climate.temperature = std::clamp(climate.temperature, -1.0f, 1.0f);

    // === HUMIDITY ===
    float humidLarge = humidityNoise.GetNoise(x, z);
    float humidMedium = humidityNoise.GetNoise(x * 3.0f, z * 3.0f) * 0.3f;
    float humidSmall = localNoise.GetNoise(x * 0.5f + 10000.0f, z * 0.5f) * 0.1f;

    climate.humidity = humidLarge + humidMedium + humidSmall;
    climate.humidity = std::clamp(climate.humidity, -1.0f, 1.0f);

    // === CONTINENTALNESS ===
    float contLarge = continentalnessNoise.GetNoise(x, z);
    float contMedium = subRegionNoise.GetNoise(x, z) * 0.2f;
    float contSmall = localNoise.GetNoise(x, z) * 0.05f;

    climate.continental = contLarge + contMedium + contSmall;
    climate.continental = std::clamp(climate.continental, -1.0f, 1.0f);

    // === EROSION ===
    float erosionLarge = erosionNoise.GetNoise(x, z);
    float erosionMedium = erosionNoise.GetNoise(x * 2.0f, z * 2.0f) * 0.3f;

    climate.erosion = erosionLarge + erosionMedium;
    climate.erosion = std::clamp(climate.erosion, -1.0f, 1.0f);

    // === PEAKS ===
    float peaksLarge = peakNoise.GetNoise(x, z);
    float peaksMedium = peakNoise.GetNoise(x * 2.0f, z * 2.0f) * 0.3f;

    climate.peaks = peaksLarge + peaksMedium;
    climate.peaks = std::clamp(climate.peaks, -1.0f, 1.0f);

    // === WEIRDNESS ===
    climate.weirdness = weirdnessNoise.GetNoise(x, z);

    return climate;
}