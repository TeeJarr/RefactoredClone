//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_RENDERER_HPP
#define REFACTOREDCLONE_RENDERER_HPP

#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>

struct UVRect {
    float u0, v0;
    float u1, v1;
};

class Renderer {
public:

     enum FACES {
        FACE_LEFT,
        FACE_RIGHT,
        FACE_TOP,
        FACE_BOTTOM,
        FACE_FRONT,
        FACE_BACK,
    };

    enum FaceTile {
        TOP_TILE = 0,
        SIDE_TILE = 1,
        BOTTOM_TILE = 2
    };

    // Mesh createCubeMesh();
    static Model &createCubeModel(Model &cubeModel, Mesh &mesh);
    // UVRect getFaceUV(int id, int face);
    static UVRect GetAtlasUV(int tileIndex);
    static int GetGrassTileForFace(int face);
    static UVRect GetTileUV(int tileIndex, int atlasWidth, int atlasHeight, int tileSize);
    static Mesh &createCubeMeshWithAtlas(Mesh &mesh);

    static std::unordered_map<std::string, Texture2D> textureMap;
    static Texture2D textureAtlas;

    static constexpr Vector2 FACE_UVS[4] = {
        {0, 0}, // top-left
        {1, 0}, // top-right
        {1, 1}, // bottom-right
        {0, 1}  // bottom-left
    };

};

#endif //REFACTOREDCLONE_RENDERER_HPP