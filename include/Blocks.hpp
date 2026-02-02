//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_BLOCKS_HPP
#define REFACTOREDCLONE_BLOCKS_HPP

#pragma once
#include <raylib.h>
#include <unordered_map>

enum BlockIds {
    ID_GRASS,
    ID_DIRT,
    ID_STONE,
    ID_AIR,
    ID_BEDROCK,
    ID_OAK_WOOD,
    ID_OAK_LEAF,
    ID_WATER,
    ID_SAND,
    ID_GLASS,
    ID_SNOW,
};

enum TextureTiles {
    GRASS_TOP_TILE = 0,
    GRASS_SIDE_TILE = 1,
    GRASS_BOTTOM_TILE = 2,
    DIRT_TILE = 2,
    STONE_TILE = 3,
    BEDROCK_TILE = 10,
    AIR_TILE = 10,
    OAK_TOP_TILE = 4,
    OAK_SIDE_TILE = 5,
    OAK_LEAF_TILE = 6,
    WATER_TILE = 9,
    SAND_TILE = 39
};


// In Blocks.hpp - update BlockTextureDef struct

struct BlockTextureDef {
    int faceTile[6];  // Texture index for each face
    bool tintTop;     // Legacy - can remove later
    Color tint;       // Tint color for the entire block
};

// Helper to create a BlockTextureDef easily
inline BlockTextureDef makeBlockDef(int all, Color tint = WHITE) {
    return {{all, all, all, all, all, all}, false, tint};
}

inline BlockTextureDef makeBlockDef(int top, int side, int bottom, Color tint = WHITE) {
    return {{side, side, side, side, top, bottom}, false, tint};
}

inline BlockTextureDef makeBlockDefWithTopTint(int top, int side, int bottom, Color tint) {
    return {{side, side, side, side, top, bottom}, true, tint};
}

// Define some useful colors
inline const Color GRASS_TINT = {105, 175, 59, 255};    // Green grass
inline const Color WATER_TINT = {60, 100, 255, 180};    // Blue water
inline const Color LEAF_TINT = {80, 140, 50, 255};      // Green leaves
inline const Color SAND_TINT = {255, 255, 255, 255};    // No tint
inline const Color SNOW_TINT = {255, 255, 255, 255};    // No tint

// Update blockTextureDefs with tints
inline std::unordered_map<BlockIds, BlockTextureDef> blockTextureDefs = {
    {ID_GRASS,      {{GRASS_SIDE_TILE, GRASS_SIDE_TILE, GRASS_SIDE_TILE, GRASS_SIDE_TILE, GRASS_TOP_TILE, DIRT_TILE}, true, GRASS_TINT}},
    {ID_DIRT,       makeBlockDef(DIRT_TILE)},
    {ID_STONE,      makeBlockDef(STONE_TILE)},
    {ID_BEDROCK,    makeBlockDef(STONE_TILE, WHITE)},
    {ID_OAK_WOOD,   makeBlockDef(OAK_TOP_TILE, OAK_SIDE_TILE, OAK_TOP_TILE)},
    {ID_OAK_LEAF,   makeBlockDef(OAK_LEAF_TILE, LEAF_TINT)},
    {ID_SAND,       makeBlockDef(SAND_TILE)},
    {ID_WATER,      makeBlockDef(WATER_TILE, WATER_TINT)},
    {ID_SNOW,       makeBlockDef(66)},
    {ID_GLASS,      makeBlockDef(49, {255, 255, 255, 128})},
    // Add more blocks...
};

// Per-face tint support (optional - for blocks like grass that only tint one face)
struct FaceTint {
    Color colors[6];  // Tint for each face
};

inline FaceTint makeFaceTint(Color all) {
    return {{all, all, all, all, all, all}};
}

inline FaceTint makeFaceTint(Color top, Color sides, Color bottom) {
    return {{sides, sides, sides, sides, top, bottom}};
}

// Optional: per-face tints for specific blocks
inline std::unordered_map<BlockIds, FaceTint> blockFaceTints = {
    {ID_GRASS, makeFaceTint(GRASS_TINT, WHITE, WHITE)},  // Only top is tinted
    {ID_OAK_LEAF, makeFaceTint(LEAF_TINT)},              // All faces tinted
    {ID_WATER, makeFaceTint(WATER_TINT)},                // All faces tinted
};

// Helper to get face tint
inline Color getBlockFaceTint(BlockIds id, int face) {
    auto it = blockFaceTints.find(id);
    if (it != blockFaceTints.end()) {
        return it->second.colors[face];
    }

    // Fall back to block-level tint
    auto defIt = blockTextureDefs.find(id);
    if (defIt != blockTextureDefs.end()) {
        return defIt->second.tint;
    }

    return WHITE;
}
// In Blocks.hpp - add these functions or a lookup table

inline bool isBlockTranslucent(int blockId) {
    switch (blockId) {
        case ID_WATER:
        case ID_OAK_LEAF:
        case ID_GLASS:  // If you have glass
            return true;
        default:
            return false;
    }
}

inline bool isBlockSolid(int blockId) {
    switch (blockId) {
        case ID_AIR:
        case ID_WATER:
            return false;
        default:
            return true;
    }
}

inline unsigned char getBlockAlpha(int blockId) {
    switch (blockId) {
        case ID_WATER:
            return 180;  // Semi-transparent
        case ID_OAK_LEAF:
            return 255;  // Fully opaque but with holes (cutout)
        case ID_GLASS:
            return 128;
        default:
            return 255;
    }
}

static std::unordered_map<int, Model> blockModels;


#endif //REFACTOREDCLONE_BLOCKS_HPP
