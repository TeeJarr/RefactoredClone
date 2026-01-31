//
// Created by Tristan on 1/30/26.
//

#include "../include/Renderer.hpp"

#include <ostream>
#include <raylib.h>

constexpr float TILE_WIDTH  = 160.0f;
constexpr float TILE_HEIGHT = 160.0f;
constexpr float ATLAS_WIDTH = 160.0f;
constexpr float ATLAS_HEIGHT = 800.0f;

Texture2D Renderer::textureAtlas = {};

UVRect Renderer::GetTileUV(int tileIndex, int atlasWidth, int atlasHeight, int tileSize) {
    float u0 = 0.0f;
    float u1 = 1.0f; // full width
    float v0 = (float)(tileIndex * tileSize) / (float)atlasHeight;
    float v1 = (float)((tileIndex + 1) * tileSize) / (float)atlasHeight;
    return {u0, v0, u1, v1};
}

Mesh& Renderer::createCubeMeshWithAtlas(Mesh& mesh) {
    mesh.vertexCount = 24;    // 6 faces × 4 vertices
    mesh.triangleCount = 12;  // 2 triangles per face × 6 faces

    // Cube vertices (0–1 in local cube space)
    float vertices[] = {
        // Front
        0,0,0, 1,0,0, 1,1,0, 0,1,0,
        // Back
        1,0,1, 0,0,1, 0,1,1, 1,1,1,
        // Left
        0,0,1, 0,0,0, 0,1,0, 0,1,1,
        // Right
        1,0,0, 1,0,1, 1,1,1, 1,1,0,
        // Top
        0,1,0, 1,1,0, 1,1,1, 0,1,1,
        // Bottom
        0,0,1, 1,0,1, 1,0,0, 0,0,0
    };

    unsigned short indices[] = {
        0,2,1, 0,3,2,      // Front
        4,6,5, 4,7,6,      // Back
        8,10,9, 8,11,10,   // Left
        12,14,13,12,15,14, // Right
        16,18,17,16,19,18, // Top
        20,22,21,20,23,22  // Bottom
    };

    float normals[] = {
        // Front
        0,0,-1,0,0,-1,0,0,-1,0,0,-1,
        // Back
        0,0,1,0,0,1,0,0,1,0,0,1,
        // Left
        -1,0,0,-1,0,0,-1,0,0,-1,0,0,
        // Right
        1,0,0,1,0,0,1,0,0,1,0,0,
        // Top
        0,1,0,0,1,0,0,1,0,0,1,0,
        // Bottom
        0,-1,0,0,-1,0,0,-1,0,0,-1,0
    };

    // Face lighting (Minecraft-style)
    float faceLight[6] = {0.9f, 0.9f, 0.8f, 0.8f, 1.0f, 0.6f};

    // UV coordinates
    Vector2 FACE_UVS[4] = {{0,0},{1,0},{1,1},{0,1}};
    float texcoords[24*2];

    // FACE order: FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM
    int faceTiles[6] = {1, 1, 1, 1, 0, 2}; // sides=1, top=0, bottom=2
    int t = 0;

    // Per-vertex colors for lighting
    unsigned char colors[24*4];
    int c = 0;

    for (int f = 0; f < 6; f++) {
        UVRect uv = Renderer::GetAtlasUV(faceTiles[f]); // Get the tile's UV rect
        unsigned char l = (unsigned char)(faceLight[f]*255.0f);
        bool flipV = (f >= 0 && f <= 3); // front/back/left/right

        for (int v = 0; v < 4; v++) {
            float u = uv.u0 + (uv.u1 - uv.u0) * FACE_UVS[v].x;
            float vCoord = uv.v0 + (uv.v1 - uv.v0) * (flipV ? 1.0f - FACE_UVS[v].y : FACE_UVS[v].y);
            texcoords[t++] = u;
            texcoords[t++] = vCoord;

            if(f == 4) {
                // Top face: tint green (mix brightness with green)
                colors[c++] = (unsigned char)(faceLight[f] * 105.0f); // Red
                colors[c++] = (unsigned char)(faceLight[f] * 175.0f); // Green (full)
                colors[c++] = (unsigned char)(faceLight[f] * 59.0f); // Blue
            } else {
                // Other faces: normal lighting grayscale
                colors[c++] = l;
                colors[c++] = l;
                colors[c++] = l;
            }
            colors[c++] = 255; // Alpha
        }
    }

    // Allocate mesh
    mesh.vertices  = (float*)MemAlloc(sizeof(vertices));
    mesh.indices   = (unsigned short*)MemAlloc(sizeof(indices));
    mesh.normals   = (float*)MemAlloc(sizeof(normals));
    mesh.texcoords = (float*)MemAlloc(sizeof(texcoords));
    mesh.colors    = (unsigned char*)MemAlloc(sizeof(colors));

    memcpy(mesh.vertices, vertices, sizeof(vertices));
    memcpy(mesh.indices, indices, sizeof(indices));
    memcpy(mesh.normals, normals, sizeof(normals));
    memcpy(mesh.texcoords, texcoords, sizeof(texcoords));
    memcpy(mesh.colors, colors, sizeof(colors));

    UploadMesh(&mesh, false);
    return mesh;
}
Model& Renderer::createCubeModel(Model& cubeModel, Mesh& mesh) {
    mesh = Renderer::createCubeMeshWithAtlas(mesh);
    cubeModel = LoadModelFromMesh(mesh);
    cubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Renderer::textureAtlas;
    return cubeModel;
}

UVRect Renderer::GetAtlasUV(int tileIndex) {
    float u0 = 0.0f;
    float u1 = 1.0f;

    float v0 = (tileIndex * TILE_HEIGHT) / ATLAS_HEIGHT;
    float v1 = ((tileIndex + 1) * TILE_HEIGHT) / ATLAS_HEIGHT;

    return { u0, v0, u1, v1 };
}

int Renderer::GetGrassTileForFace(int face) {
    switch (face) {
        case FACE_TOP:    return 0; // grass top
        case FACE_BOTTOM: return 2; // dirt
        default:          return 1; // grass side
    }
}



