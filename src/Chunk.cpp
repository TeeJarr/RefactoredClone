//
// Created by Tristan on 1/30/26.
//

#include "../include/Chunk.hpp"

#include <algorithm>
#include "LightingSystem.hpp"

#include "Blocks.hpp"
#include "Settings.hpp"
#include <print>
#include <raymath.h>

constexpr int WATER_LEVEL = 50;
constexpr int BEACH_LEVEL = 51;

void ChunkHelper::generateChunkTerrain(const std::unique_ptr<Chunk> &chunk) {
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
                chunk->blockPosition[x][(int) surfaceY][z] = ID_WATER;
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
    const Biome& biome = biomes.at(biomeType);

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
    float val = (biomeNoise.GetNoise(wx * 0.001f, wz * 0.001f) + 1.0f) * 0.5f; // low-frequency biome noise
    if (val < 0.4f) return BIOME_PLAINS;
    if (val < 0.7f) return BIOME_HILLS;
    return BIOME_MOUNTAINS;
}

float ChunkHelper::getChunkHeight(const Vector3 &worldPosition) {
    float scale = 0.f;
    float height = ChunkHelper::terrainNoise.GetNoise(
        worldPosition.x * scale,
        worldPosition.y * scale,
        worldPosition.z * scale
    );

    height = (height + 1.0f) * 0.5f;

    return height * 64.0f + 64.0f;
}

void ChunkHelper::initNoiseRenderer() {
    // Tree noise
    treeNoise.SetSeed(Settings::worldSeed + 1337);
    treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    treeNoise.SetFrequency(0.01f);

    // Cave noise
    caveNoise.SetSeed(Settings::worldSeed + 9001);
    caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    caveNoise.SetFrequency(0.08f);

    // Terrain noise
    terrainNoise.SetSeed(Settings::worldSeed);
    terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise.SetFractalOctaves(5);
    terrainNoise.SetFractalLacunarity(2.0f);
    terrainNoise.SetFractalGain(0.5f);
    terrainNoise.SetFrequency(0.008f);  // Main terrain frequency

    // Biome noise
    biomeNoise.SetSeed(static_cast<int>(Settings::worldSeed + 1024));
    biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeNoise.SetFractalOctaves(5);
    biomeNoise.SetFractalLacunarity(2.0f);
    biomeNoise.SetFractalGain(0.5f);
    biomeNoise.SetFrequency(0.004f);  // Biome regions

    // Temperature noise
    temperatureNoise.SetSeed(static_cast<int>(Settings::worldSeed + 2000));
    temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    temperatureNoise.SetFractalOctaves(5);
    temperatureNoise.SetFractalLacunarity(2.0f);
    temperatureNoise.SetFractalGain(0.5f);
    temperatureNoise.SetFrequency(0.002f);  // Large temperature zones

    // Humidity noise
    humidityNoise.SetSeed(static_cast<int>(Settings::worldSeed + 3000));
    humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    humidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    humidityNoise.SetFractalOctaves(5);
    humidityNoise.SetFractalLacunarity(2.0f);
    humidityNoise.SetFractalGain(0.5f);
    humidityNoise.SetFrequency(0.002f);  // Large humidity zones

    // Continentalness noise
    continentalnessNoise.SetSeed(static_cast<int>(Settings::worldSeed + 4000));
    continentalnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continentalnessNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentalnessNoise.SetFractalOctaves(5);
    continentalnessNoise.SetFractalLacunarity(2.0f);
    continentalnessNoise.SetFractalGain(0.5f);
    continentalnessNoise.SetFrequency(0.001f);  // Very large continents
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

Chunk *ChunkHelper::getChunkFromWorld(int wx, int wz) {
    int cx = floorDiv(wx, CHUNK_SIZE_X);
    int cz = floorDiv(wz, CHUNK_SIZE_Z);

    ChunkCoord coord{ cx, cz };

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end())
        return nullptr;

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

 ChunkCoord ChunkHelper::worldToChunkCoord(int wx, int wz){
    auto conv = [](int v, int size) {
        return (v >= 0) ? (v / size) : ((v - size + 1) / size);
    };

    return {
        conv(wx, CHUNK_SIZE_X),
        conv(wz, CHUNK_SIZE_Z)
    };
}

void ChunkHelper::markChunkDirty(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(activeChunksMutex);

    auto it = activeChunks.find(coord);
    if (it == activeChunks.end()) return;

    Chunk* chunk = it->second.get();
    if (!chunk) return;
    if (chunk->dirty) return;  // Already dirty
    if (chunk->meshBuilding.load()) return;  // Already rebuilding

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
    float freq = 0.5f; // large-scale features
    float val = biomeNoise.GetNoise(wx * freq, 0.0f, wz * freq); // -1..1
    val = (val + 1.0f) / 2.0f; // 0..1
    return val;
}

Biome ChunkHelper::getBiomeType(float biomeFactor) {
    if (biomeFactor < 0.6) {
        return biomes.at(BIOME_PLAINS);
    } else if (biomeFactor < 0.85) {
        return biomes.at(BIOME_HILLS);
    } else {
        return biomes.at(BIOME_MOUNTAINS);
    }


}

float ChunkHelper::getSurfaceHeight(int wx, int wz) {
    BiomeType biomeType = getBiomeAt(wx, wz);
    const Biome& biome = biomes.at(biomeType);

    float x = static_cast<float>(wx);
    float z = static_cast<float>(wz);

    // Base continental shape
    float continent = continentalnessNoise.GetNoise(x, z);
    continent = (continent + 1.0f) * 0.5f;

    // Biome-specific terrain
    float terrainHeight = terrainNoise.GetNoise(x * biome.hillFrequency, z * biome.hillFrequency);
    terrainHeight = (terrainHeight + 1.0f) * 0.5f;

    // Detail noise
    float detail = terrainNoise.GetNoise(x, z);
    detail = (detail + 1.0f) * 0.5f;

    // Combine
    float height = biome.baseHeight;
    height += terrainHeight * biome.heightVariation;
    height += detail * 3.0f;

    // Extra mountain features
    if (biomeType == BIOME_MOUNTAINS) {
        // Ridged noise for jagged peaks
        float ridge = terrainNoise.GetNoise(x * 0.02f + 2000.0f, z * 0.02f + 2000.0f);
        ridge = 1.0f - fabs(ridge);
        ridge = ridge * ridge;
        height += ridge * 25.0f;
    }

    // Ocean floor variation
    if (biomeType == BIOME_OCEAN) {
        height = std::min(height, (float)(WATER_LEVEL - 5));
    }

    // Blend biomes at edges for smooth transitions
    // Sample neighboring biomes and average
    float blendRadius = 8.0f;
    float totalWeight = 1.0f;
    float blendedHeight = height;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;

            int sampleX = wx + static_cast<int>(dx * blendRadius);
            int sampleZ = wz + static_cast<int>(dz * blendRadius);

            BiomeType neighborBiome = getBiomeAt(sampleX, sampleZ);
            if (neighborBiome != biomeType) {
                const Biome& neighbor = biomes.at(neighborBiome);

                float neighborTerrain = terrainNoise.GetNoise(
                    sampleX * neighbor.hillFrequency,
                    sampleZ * neighbor.hillFrequency
                );
                neighborTerrain = (neighborTerrain + 1.0f) * 0.5f;

                float neighborHeight = neighbor.baseHeight + neighborTerrain * neighbor.heightVariation;

                float weight = 0.3f;
                blendedHeight += neighborHeight * weight;
                totalWeight += weight;
            }
        }
    }

    height = blendedHeight / totalWeight;

    return std::max(height, 1.0f);
}

std::unique_ptr<Chunk> ChunkHelper::generateChunkAsync(const Vector3 &chunkPosIndex) {
    auto chunk = std::make_unique<Chunk>();

    int chunkX = static_cast<int>(chunkPosIndex.x);
    int chunkZ = static_cast<int>(chunkPosIndex.z);
    chunk->chunkCoords.x = chunkX;
    chunk->chunkCoords.z = chunkZ;

    generateChunkTerrain(chunk);  // terrain + caves
    setBiomeFloor(chunk);          // grass/dirt/sand
    populateTrees(*chunk);         // trees

    chunk->boundingBox.min = { (float)chunkX * CHUNK_SIZE_X, 0.0f, (float)chunkZ * CHUNK_SIZE_Z };
    chunk->boundingBox.max = { (float)chunkX * CHUNK_SIZE_X + CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)chunkZ * CHUNK_SIZE_Z + CHUNK_SIZE_Z };

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
        return ID_AIR;  // Assume air if chunk not loaded
    }

    int localX = WorldToLocal(worldX);
    int localZ = WorldToLocal(worldZ);

    return static_cast<BlockIds>(it->second->blockPosition[localX][worldY][localZ]);
}


void ChunkHelper::setBiomeFloor(const std::unique_ptr<Chunk> &chunk) {
    int baseX = chunk->chunkCoords.x * CHUNK_SIZE_X;
    int baseZ = chunk->chunkCoords.z * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = baseX + x;
            int wz = baseZ + z;
            int surfaceY = static_cast<int>(chunk->surfaceHeight[x][z]);

            BiomeType biomeType = getBiomeAt(wx, wz);
            const Biome& biome = biomes.at(biomeType);

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
void ChunkHelper::populateTrees(Chunk &chunk) {
    int baseX = chunk.chunkCoords.x * CHUNK_SIZE_X;
    int baseZ = chunk.chunkCoords.z * CHUNK_SIZE_Z;

    for (int lx = 2; lx < CHUNK_SIZE_X - 2; lx++) {  // Avoid edges
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

                        if (nx >= 0 && nx < CHUNK_SIZE_X &&
                            ny < CHUNK_SIZE_Y &&
                            nz >= 0 && nz < CHUNK_SIZE_Z) {
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
    std::println("Generating trees...");
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

float ChunkHelper::LERP(float a, float b, float t) {
    return a + t * (b - a);
}




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
BiomeType ChunkHelper::getBiomeAt(int wx, int wz) {
    float continentalness = getContinentalness(wx, wz);
    float temperature = getTemperature(wx, wz);
    float humidity = getHumidity(wx, wz);

    // Ocean (low continentalness)
    if (continentalness < 0.3f) {
        return BIOME_OCEAN;
    }

    // Beach (edge of ocean)
    if (continentalness < 0.38f) {
        return BIOME_BEACH;
    }

    // Mountains (use separate noise for peaks)
    float mountainNoise = biomeNoise.GetNoise(wx * 0.004f, wz * 0.004f);
    mountainNoise = (mountainNoise + 1.0f) * 0.5f;
    if (mountainNoise > 0.7f && continentalness > 0.5f) {
        return BIOME_MOUNTAINS;
    }

    // Hills
    if (mountainNoise > 0.55f && continentalness > 0.45f) {
        return BIOME_HILLS;
    }

    // Desert (hot and dry)
    if (temperature > 0.6f && humidity < 0.3f) {
        return BIOME_DESERT;
    }

    // Forest (humid)
    if (humidity > 0.5f) {
        return BIOME_FOREST;
    }

    // Default plains
    return BIOME_PLAINS;
}