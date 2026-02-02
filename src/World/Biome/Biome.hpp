//
// Created by Tristan on 2/2/26.
//

#ifndef REFACTOREDCLONE_BIOME_HPP
#define REFACTOREDCLONE_BIOME_HPP
#include <string>

#include "Chunk/Chunk.hpp"


class BiomesHelper {
    public:
    static std::string getBiomeName(BiomeType biomeType);
};


#endif //REFACTOREDCLONE_BIOME_HPP