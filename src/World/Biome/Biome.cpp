//
// Created by Tristan on 2/2/26.
//

#include "Biome.hpp"

#include "Chunk/Chunk.hpp"

std::string BiomesHelper::getBiomeName(BiomeType biomeType) {
    static const std::unordered_map<BiomeType, std::string> biomeNames = {
        {BiomeType::BIOME_OCEAN, "Ocean"},
        {BiomeType::BIOME_BEACH, "Beach"},
        {BiomeType::BIOME_DESERT, "Desert"},
        {BiomeType::BIOME_PLAINS, "Plains"},
        {BiomeType::BIOME_FOREST, "Forest"},
        {BiomeType::BIOME_TAIGA, "Taiga"},
        {BiomeType::BIOME_SWAMP, "Swamp"},
        {BiomeType::BIOME_HILLS, "Hills"},
        {BiomeType::BIOME_MOUNTAINS, "Mountains"},
        {BiomeType::BIOME_RIVER, "River"},
    };

    auto it = biomeNames.find(biomeType);
    if (it != biomeNames.end()) {
        return it->second;
    }
    return "Unknown";
}