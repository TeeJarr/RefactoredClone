//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_RENDERER_HPP
#define REFACTOREDCLONE_RENDERER_HPP

#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>

#include "Blocks.hpp"
#include "Chunk.hpp"

struct UVRect {
    float u0, v0;
    float u1, v1;
};


struct Plane {
    Vector3 normal;
    float distance;
};

struct Frustum {
    Plane rightFace;
    Plane leftFace;

    Plane bottomFace;
    Plane topFace;

    Plane nearFace;
    Plane farFace;
};

// FACE ORDER MUST MATCH FACE_TEMPLATES
static const int dx[6] = {  0,  0, -1,  1,  0,  0 };
static const int dy[6] = {  0,  0,  0,  0,  1, -1 };
static const int dz[6] = { -1,  1,  0,  0,  0,  0 };

// In Renderer.hpp - add these declarations

struct FaceTemplate {
    Vector3 verts[4];
    Vector3 normal;
};

// FACE ORDER: FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM
static const FaceTemplate FACE_TEMPLATES[6] = {
    // FRONT (−Z)
    {{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}, {0,0,-1}},

    // BACK (+Z)
    {{{1,0,1},{1,1,1},{0,1,1},{0,0,1}}, {0,0,1}},

    // LEFT (−X)
    {{{0,0,1},{0,1,1},{0,1,0},{0,0,0}}, {-1,0,0}},

    // RIGHT (+X)
    {{{1,0,0},{1,1,0},{1,1,1},{1,0,1}}, {1,0,0}},

    // TOP (+Y)
    {{{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, {0,1,0}},

    // BOTTOM (−Y)
    {{{0,0,1},{0,0,0},{1,0,0},{1,0,1}}, {0,-1,0}}
};

inline float FACE_LIGHT[6] = {0.9f, 0.9f, 0.8f, 0.8f, 1.0f, 0.6f};
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

    static std::vector<std::thread> workers;

    static UVRect GetAtlasUV(int tileIndex, int atlasWidth, int atlasHeight, int tileWidth, int tileHeight);

     static void initChunkWorkers(int threadCount);

     static void replaceChunk(const ChunkCoord &coord, std::unique_ptr<Chunk> newChunk);

    static UVRect GetTileUV(int tileIndex, int atlasWidth, int atlasHeight, int tileSize);

    static bool isFaceExposed(const Chunk &chunk, int x, int y, int z, int face);

     static Plane normalizePlane(const Plane &plane);

     static bool isBoxOnScreen(const BoundingBox &box, const Camera3D &camera);

     static Vector3 WorldToCamera(const Vector3 &p, const Camera3D &cam);

    static void GetCameraFrustumPlanes(const Camera3D& cam, Plane planes[6]);

    static bool IsBoxInFrustum(const BoundingBox& box, const Plane planes[6]);

     static void AddFace(ChunkMeshBuffers &buf, const Vector3 &blockPos, int face, const BlockTextureDef &def, float lightLevel, bool tintTop);

     static void buildChunkModel(const Chunk &chunk);

     static void checkActiveChunks(const Camera3D &camera);

     static void unloadChunks(const Camera3D &camera_3d);

     static int worldToChunk(float v, int chunkSize);

     static void drawChunk(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera);

    static void renderAsyncChunks();


     static void chunkWorkerThread();

     static void shutdownChunkWorkers();

     static void shutdown();

     static void processChunkBuildQueue(const Camera3D &camera);

     static ChunkCoord getPlayerChunkCoord(const Camera3D &camera);

    static void drawCrosshair();

     static std::unordered_map<std::string, Texture2D> textureMap;
    static Texture2D textureAtlas;

    static constexpr Vector2 FACE_UVS[4] = {
        {0, 0}, // top-left
        {1, 0}, // top-right
        {1, 1}, // bottom-right
        {0, 1}  // bottom-left
    };


    // In Chunk.hpp
    static void rebuildDirtyChunks();

     static bool isFaceExposed(const Chunk &chunk, int x, int y, int z, int face, bool isTranslucent);

    static ChunkMeshTriple buildChunkMeshes(const Chunk &chunk);

     static void AddFaceWithAlpha(ChunkMeshBuffers &buf, const Vector3 &blockPos, int face, const BlockTextureDef &def,
                                  float lightLevel, Color tint, unsigned char alpha);

    static Model buildModelFromBuffers(ChunkMeshBuffers &buf);
    static void drawChunkOpaque(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera);
    static void drawChunkTranslucent(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera);

     static void drawChunkWater(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera);

     static void drawAllChunks(const Camera3D &camera);

    // In Renderer.hpp
    static float waterAnimTime;
    static int waterAnimFrame;
    static constexpr int WATER_FRAME_COUNT = 32;  // Number of water frames in atlas
    static constexpr int WATER_FIRST_TILE = 8;    // First water tile index
    static constexpr float WATER_FRAME_DURATION = 0.05f;  // Seconds per frame

    // In Renderer.hpp
    static Shader waterShader;
    static int waterTimeLoc;

    static void initWaterShader();
    static void updateWaterShader(float time);};

// In Renderer.hpp - add these declarations



inline int numThreads = 4;

#endif //REFACTOREDCLONE_RENDERER_HPP