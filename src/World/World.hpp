//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_WORLD_HPP
#define REFACTOREDCLONE_WORLD_HPP
#include <cstdint>


struct WorldHeader {
    uint32_t version;
    uint32_t seed;
};

class World {
public:


private:
    WorldHeader header{};
};

#endif //REFACTOREDCLONE_WORLD_HPP
