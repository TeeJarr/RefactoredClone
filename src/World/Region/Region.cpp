//
// Created by Claude - Region-based biome system implementation
//

#include "Region.hpp"
#include "../Chunk/Chunk.hpp"
#include <algorithm>
#include <cmath>

RegionParams Region::sampleAt(int worldX, int worldZ) {
    RegionParams params;
    params.continentalness = getContinentalness(worldX, worldZ);
    params.temperature = getTemperature(worldX, worldZ);
    params.humidity = getHumidity(worldX, worldZ);
    params.erosion = getErosion(worldX, worldZ);
    params.weirdness = getWeirdness(worldX, worldZ);
    params.depth = 0.0f; // Not used for surface biomes
    return params;
}

float Region::getContinentalness(int worldX, int worldZ) {
    float raw = ChunkHelper::continentalnessNoise.GetNoise((float)worldX, (float)worldZ);
    // Apply slight amplification to make continents more distinct
    return std::clamp(raw * 1.2f, -1.0f, 1.0f);
}

float Region::getTemperature(int worldX, int worldZ) {
    float raw = ChunkHelper::temperatureNoise.GetNoise((float)worldX, (float)worldZ);
    return std::clamp(raw, -1.0f, 1.0f);
}

float Region::getHumidity(int worldX, int worldZ) {
    float raw = ChunkHelper::humidityNoise.GetNoise((float)worldX, (float)worldZ);
    return std::clamp(raw, -1.0f, 1.0f);
}

float Region::getErosion(int worldX, int worldZ) {
    float raw = ChunkHelper::erosionNoise.GetNoise((float)worldX, (float)worldZ);
    return std::clamp(raw, -1.0f, 1.0f);
}

float Region::getWeirdness(int worldX, int worldZ) {
    float raw = ChunkHelper::peakNoise.GetNoise((float)worldX, (float)worldZ);
    return std::clamp(raw, -1.0f, 1.0f);
}

float Region::getTerrainFactor(const RegionParams& params) {
    // Continentalness affects base elevation
    // More inland = higher base terrain
    float continentFactor = (params.continentalness + 1.0f) * 0.5f; // 0 to 1

    // Erosion affects terrain roughness
    // Lower erosion = flatter, higher = more varied
    float erosionFactor = (params.erosion + 1.0f) * 0.5f; // 0 to 1

    return continentFactor * (0.5f + erosionFactor * 0.5f);
}

float Region::getPeakValleyFactor(const RegionParams& params) {
    // Weirdness determines peaks vs valleys
    // Combined with erosion for mountain placement
    float weirdFactor = params.weirdness;
    float erosionBonus = std::max(0.0f, params.erosion) * 0.3f;

    return weirdFactor + erosionBonus;
}

float Region::getTerrainHeight(const RegionParams& params, int worldX, int worldZ) {
    const float SEA_LEVEL = 62.0f;
    const float BASE_HEIGHT = 64.0f;

    // Sample detail noise for local variation
    float terrain = ChunkHelper::terrainNoise.GetNoise((float)worldX, (float)worldZ);
    float detail = ChunkHelper::detailNoise.GetNoise((float)worldX, (float)worldZ);

    float height;

    // Deep ocean
    if (params.continentalness < DEEP_OCEAN_THRESHOLD) {
        height = 35.0f + terrain * 8.0f + detail * 2.0f;
        return std::clamp(height, 25.0f, 45.0f);
    }

    // Ocean
    if (params.continentalness < OCEAN_THRESHOLD) {
        float oceanDepth = (OCEAN_THRESHOLD - params.continentalness) /
                          (OCEAN_THRESHOLD - DEEP_OCEAN_THRESHOLD);
        height = SEA_LEVEL - 5.0f - oceanDepth * 15.0f + terrain * 5.0f;
        return std::clamp(height, 35.0f, SEA_LEVEL - 2.0f);
    }

    // Coast/Beach
    if (params.continentalness < COAST_THRESHOLD) {
        float coastBlend = (params.continentalness - OCEAN_THRESHOLD) /
                          (COAST_THRESHOLD - OCEAN_THRESHOLD);
        height = SEA_LEVEL - 2.0f + coastBlend * 4.0f + terrain * 2.0f;
        return std::clamp(height, SEA_LEVEL - 3.0f, SEA_LEVEL + 3.0f);
    }

    // Inland terrain
    float baseInland = BASE_HEIGHT;

    // Apply erosion-based flatness
    float flatness = 1.0f - (params.erosion + 1.0f) * 0.3f; // 0.4 to 1.0
    flatness = std::clamp(flatness, 0.3f, 1.0f);

    // Base terrain variation
    height = baseInland + terrain * 12.0f * flatness + detail * 3.0f;

    // Hills based on erosion and weirdness
    if (params.erosion > EROSION_NORMAL && params.weirdness > LOW_THRESHOLD) {
        float hillFactor = (params.erosion - EROSION_NORMAL) / (1.0f - EROSION_NORMAL);
        float weirdFactor = (params.weirdness - LOW_THRESHOLD) / (1.0f - LOW_THRESHOLD);
        height += hillFactor * weirdFactor * 20.0f;
    }

    // Mountains when both erosion and weirdness are high
    if (params.erosion > EROSION_HILLY && params.weirdness > MID_THRESHOLD) {
        float mountainFactor = (params.erosion - EROSION_HILLY) / (1.0f - EROSION_HILLY);
        float peakFactor = (params.weirdness - MID_THRESHOLD) / (1.0f - MID_THRESHOLD);
        height += mountainFactor * peakFactor * 35.0f;

        // Extreme peaks
        if (params.weirdness > HIGH_THRESHOLD) {
            float extremeFactor = (params.weirdness - HIGH_THRESHOLD) / (1.0f - HIGH_THRESHOLD);
            height += extremeFactor * extremeFactor * 25.0f;
        }
    }

    // Valleys when weirdness is negative
    if (params.weirdness < VALLEY_THRESHOLD) {
        float valleyFactor = (VALLEY_THRESHOLD - params.weirdness) / (VALLEY_THRESHOLD + 1.0f);
        height -= valleyFactor * 10.0f;
        height = std::max(height, SEA_LEVEL + 1.0f);
    }

    return std::clamp(height, 1.0f, 251.0f);
}

int Region::selectBiome(const RegionParams& params) {
    // Ocean biomes
    if (params.continentalness < OCEAN_THRESHOLD) {
        return selectOceanBiome(params);
    }

    // Beach biomes (coast)
    if (params.continentalness < COAST_THRESHOLD) {
        return selectBeachBiome(params);
    }

    // Mountain biomes (high erosion + high weirdness)
    if (params.erosion > EROSION_HILLY && params.weirdness > MID_THRESHOLD) {
        return selectMountainBiome(params);
    }

    // Land biomes
    return selectLandBiome(params);
}

int Region::selectOceanBiome(const RegionParams& params) {
    // Deep ocean vs regular ocean
    if (params.continentalness < DEEP_OCEAN_THRESHOLD) {
        if (params.temperature < FROZEN_THRESHOLD) {
            return BIOME_OCEAN; // Frozen deep ocean would go here
        }
        return BIOME_OCEAN;
    }

    // Regular ocean - could add warm/cold variants
    if (params.temperature < FROZEN_THRESHOLD) {
        return BIOME_OCEAN; // Frozen ocean
    }
    return BIOME_OCEAN;
}

int Region::selectBeachBiome(const RegionParams& params) {
    // Hot + dry = desert beach
    if (params.temperature > WARM_THRESHOLD && params.humidity < DRY_THRESHOLD) {
        return BIOME_DESERT;
    }

    // Cold = snowy beach (use taiga for now)
    if (params.temperature < COLD_THRESHOLD) {
        return BIOME_TAIGA;
    }

    return BIOME_BEACH;
}

int Region::selectMountainBiome(const RegionParams& params) {
    // Extreme mountains
    if (params.weirdness > HIGH_THRESHOLD) {
        return BIOME_MOUNTAINS;
    }

    // Cold mountains
    if (params.temperature < COLD_THRESHOLD) {
        return BIOME_MOUNTAINS; // Could be snowy peaks
    }

    // Hills vs mountains based on erosion
    if (params.erosion < EROSION_HILLY + 0.2f) {
        return BIOME_HILLS;
    }

    return BIOME_MOUNTAINS;
}

int Region::selectLandBiome(const RegionParams& params) {
    // Temperature-based biome selection

    // Frozen biomes
    if (params.temperature < FROZEN_THRESHOLD) {
        if (params.humidity > WET_THRESHOLD) {
            return BIOME_TAIGA; // Snowy taiga
        }
        return BIOME_TAIGA; // Snowy plains would go here
    }

    // Cold biomes
    if (params.temperature < COLD_THRESHOLD) {
        if (params.humidity > NEUTRAL_THRESHOLD) {
            return BIOME_TAIGA;
        }
        return BIOME_PLAINS; // Cold plains
    }

    // Temperate biomes
    if (params.temperature < TEMPERATE_THRESHOLD) {
        if (params.humidity > WET_THRESHOLD) {
            // Check for swamp conditions (wet + low elevation tendency)
            if (params.weirdness < LOW_THRESHOLD && params.erosion < EROSION_NORMAL) {
                return BIOME_SWAMP;
            }
            return BIOME_FOREST;
        }
        if (params.humidity > NEUTRAL_THRESHOLD) {
            return BIOME_FOREST;
        }
        if (params.humidity < ARID_THRESHOLD) {
            return BIOME_PLAINS; // Dry plains
        }
        return BIOME_PLAINS;
    }

    // Warm biomes
    if (params.temperature < WARM_THRESHOLD) {
        if (params.humidity > WET_THRESHOLD) {
            return BIOME_SWAMP; // Could be jungle
        }
        if (params.humidity > NEUTRAL_THRESHOLD) {
            return BIOME_FOREST;
        }
        if (params.humidity < ARID_THRESHOLD) {
            return BIOME_DESERT;
        }
        return BIOME_PLAINS;
    }

    // Hot biomes
    if (params.humidity > WET_THRESHOLD) {
        return BIOME_SWAMP; // Jungle would go here
    }
    if (params.humidity > NEUTRAL_THRESHOLD) {
        return BIOME_FOREST; // Sparse jungle
    }
    if (params.humidity < DRY_THRESHOLD) {
        return BIOME_DESERT;
    }
    return BIOME_DESERT; // Savanna would go here
}
