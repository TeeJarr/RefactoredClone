//
// Created by Claude - Region-based biome system similar to Minecraft
//

#ifndef REFACTOREDCLONE_REGION_HPP
#define REFACTOREDCLONE_REGION_HPP

#pragma once
#include <cstdint>
#include <algorithm>

// Regional climate parameters sampled at a world position
// These control biome selection similar to Minecraft's multi-noise biome source
struct RegionParams {
    float continentalness;  // -1 to 1: ocean (-1) to inland (1)
    float temperature;      // -1 to 1: frozen (-1) to hot (1)
    float humidity;         // -1 to 1: dry (-1) to wet (1)
    float erosion;          // -1 to 1: flat (-1) to eroded/rough (1)
    float weirdness;        // -1 to 1: normal (-1) to weird/peaks (1)
    float depth;            // For 3D biomes (caves) - not used yet
};

// Biome categories based on Minecraft's system
enum class BiomeCategory {
    OCEAN,
    BEACH,
    DESERT,
    BADLANDS,
    PLAINS,
    FOREST,
    TAIGA,
    SNOWY,
    SWAMP,
    JUNGLE,
    MOUNTAINS,
    RIVER
};

// Region class handles sampling climate parameters and biome selection
class Region {
public:
    // Sample all regional parameters at a world position
    static RegionParams sampleAt(int worldX, int worldZ);

    // Get continentalness (-1 = deep ocean, 0 = coast, 1 = inland)
    static float getContinentalness(int worldX, int worldZ);

    // Get temperature (-1 = frozen, 0 = temperate, 1 = hot)
    static float getTemperature(int worldX, int worldZ);

    // Get humidity (-1 = arid, 0 = normal, 1 = humid)
    static float getHumidity(int worldX, int worldZ);

    // Get erosion (-1 = flat, 1 = eroded/mountainous potential)
    static float getErosion(int worldX, int worldZ);

    // Get weirdness/peaks (-1 = valleys, 0 = normal, 1 = peaks)
    static float getWeirdness(int worldX, int worldZ);

    // Calculate base terrain height from regional parameters
    static float getTerrainHeight(const RegionParams& params, int worldX, int worldZ);

    // Select biome based on regional parameters (Minecraft-style)
    static int selectBiome(const RegionParams& params);

    // Terrain factor calculations
    static float getTerrainFactor(const RegionParams& params);
    static float getPeakValleyFactor(const RegionParams& params);

private:
    // Biome selection helper functions
    static int selectOceanBiome(const RegionParams& params);
    static int selectBeachBiome(const RegionParams& params);
    static int selectLandBiome(const RegionParams& params);
    static int selectMountainBiome(const RegionParams& params);

    // Continentalness thresholds (similar to Minecraft)
    static constexpr float DEEP_OCEAN_THRESHOLD = -0.45f;
    static constexpr float OCEAN_THRESHOLD = -0.2f;
    static constexpr float COAST_THRESHOLD = -0.1f;
    static constexpr float NEAR_INLAND_THRESHOLD = 0.03f;
    static constexpr float INLAND_THRESHOLD = 0.3f;
    static constexpr float FAR_INLAND_THRESHOLD = 0.55f;

    // Temperature thresholds
    static constexpr float FROZEN_THRESHOLD = -0.45f;
    static constexpr float COLD_THRESHOLD = -0.15f;
    static constexpr float TEMPERATE_THRESHOLD = 0.2f;
    static constexpr float WARM_THRESHOLD = 0.55f;
    // Above WARM = hot

    // Humidity thresholds
    static constexpr float ARID_THRESHOLD = -0.35f;
    static constexpr float DRY_THRESHOLD = -0.1f;
    static constexpr float NEUTRAL_THRESHOLD = 0.1f;
    static constexpr float WET_THRESHOLD = 0.3f;
    // Above WET = humid

    // Erosion thresholds
    static constexpr float EROSION_FLAT = -0.4f;
    static constexpr float EROSION_NORMAL = 0.0f;
    static constexpr float EROSION_HILLY = 0.35f;
    // Above HILLY = mountainous

    // Weirdness/peaks thresholds
    static constexpr float VALLEY_THRESHOLD = -0.4f;
    static constexpr float LOW_THRESHOLD = -0.05f;
    static constexpr float MID_THRESHOLD = 0.05f;
    static constexpr float HIGH_THRESHOLD = 0.4f;
    // Above HIGH = peaks
};

#endif // REFACTOREDCLONE_REGION_HPP
