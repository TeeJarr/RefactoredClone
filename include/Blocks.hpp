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
    SAND_TILE = 19
};


struct BlockTextureDef {
    int faceTile[6]; // FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM
    bool tintTop;
};

static const std::unordered_map<BlockIds, BlockTextureDef> blockTextureDefs = {
    {
        BlockIds::ID_GRASS,
        {
            {
                GRASS_SIDE_TILE, GRASS_SIDE_TILE, GRASS_SIDE_TILE, GRASS_SIDE_TILE,
                GRASS_TOP_TILE, DIRT_TILE
            },
            true
        }
    },
    {
        BlockIds::ID_DIRT,
        {
            {
                DIRT_TILE, DIRT_TILE, DIRT_TILE, DIRT_TILE,
                DIRT_TILE, DIRT_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_STONE,
        {
            {
                STONE_TILE, STONE_TILE, STONE_TILE, STONE_TILE,
                STONE_TILE, STONE_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_BEDROCK,
        {
            {
                STONE_TILE, STONE_TILE, STONE_TILE, STONE_TILE,
                STONE_TILE, STONE_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_AIR,
        {
            {
                AIR_TILE, AIR_TILE, AIR_TILE, AIR_TILE,
                AIR_TILE, AIR_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_OAK_WOOD,
        {
            {
                OAK_SIDE_TILE, OAK_SIDE_TILE, OAK_SIDE_TILE, OAK_SIDE_TILE,
                OAK_TOP_TILE, OAK_TOP_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_OAK_LEAF, {
            {
                OAK_LEAF_TILE, OAK_LEAF_TILE, OAK_LEAF_TILE, OAK_LEAF_TILE,
                OAK_LEAF_TILE, OAK_LEAF_TILE
            },
            true
        }
    },
    {
        BlockIds::ID_WATER, {
            {
                WATER_TILE, WATER_TILE, WATER_TILE, WATER_TILE,
                WATER_TILE, WATER_TILE
            },
            false
        }
    },
    {
        BlockIds::ID_SAND, {
            {
                SAND_TILE, SAND_TILE, SAND_TILE, SAND_TILE,
                SAND_TILE, SAND_TILE
            },
            false
        }
    }
};

static std::unordered_map<int, Model> blockModels;


#endif //REFACTOREDCLONE_BLOCKS_HPP
