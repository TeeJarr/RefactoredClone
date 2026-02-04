//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_RENDERER_HPP
#define REFACTOREDCLONE_RENDERER_HPP

#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>

#include "../../include/Block/Blocks.hpp"
#include "Chunk/Chunk.hpp"
#include "../../include/Common.hpp"

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

struct NeighborEdgeData {
    std::unique_ptr<uint8_t[]> blocksNegX;
    std::unique_ptr<uint8_t[]> blocksPosX;
    std::unique_ptr<uint8_t[]> blocksNegZ;
    std::unique_ptr<uint8_t[]> blocksPosZ;

    std::unique_ptr<uint8_t[]> lightNegX;
    std::unique_ptr<uint8_t[]> lightPosX;
    std::unique_ptr<uint8_t[]> lightNegZ;
    std::unique_ptr<uint8_t[]> lightPosZ;

    bool hasNegX = false;
    bool hasPosX = false;
    bool hasNegZ = false;
    bool hasPosZ = false;

    NeighborEdgeData() {
        blocksNegX = std::make_unique<uint8_t[]>(CHUNK_SIZE_Y * CHUNK_SIZE_Z);
        blocksPosX = std::make_unique<uint8_t[]>(CHUNK_SIZE_Y * CHUNK_SIZE_Z);
        blocksNegZ = std::make_unique<uint8_t[]>(CHUNK_SIZE_X * CHUNK_SIZE_Y);
        blocksPosZ = std::make_unique<uint8_t[]>(CHUNK_SIZE_X * CHUNK_SIZE_Y);

        lightNegX = std::make_unique<uint8_t[]>(CHUNK_SIZE_Y * CHUNK_SIZE_Z);
        lightPosX = std::make_unique<uint8_t[]>(CHUNK_SIZE_Y * CHUNK_SIZE_Z);
        lightNegZ = std::make_unique<uint8_t[]>(CHUNK_SIZE_X * CHUNK_SIZE_Y);
        lightPosZ = std::make_unique<uint8_t[]>(CHUNK_SIZE_X * CHUNK_SIZE_Y);
    }

    // Helper accessors
    inline uint8_t getBlockNegX(int y, int z) const { return blocksNegX[y * CHUNK_SIZE_Z + z]; }
    inline uint8_t getBlockPosX(int y, int z) const { return blocksPosX[y * CHUNK_SIZE_Z + z]; }
    inline uint8_t getBlockNegZ(int x, int y) const { return blocksNegZ[x * CHUNK_SIZE_Y + y]; }
    inline uint8_t getBlockPosZ(int x, int y) const { return blocksPosZ[x * CHUNK_SIZE_Y + y]; }

    inline uint8_t getLightNegX(int y, int z) const { return lightNegX[y * CHUNK_SIZE_Z + z]; }
    inline uint8_t getLightPosX(int y, int z) const { return lightPosX[y * CHUNK_SIZE_Z + z]; }
    inline uint8_t getLightNegZ(int x, int y) const { return lightNegZ[x * CHUNK_SIZE_Y + y]; }
    inline uint8_t getLightPosZ(int x, int y) const { return lightPosZ[x * CHUNK_SIZE_Y + y]; }

    inline void setBlockNegX(int y, int z, uint8_t v) { blocksNegX[y * CHUNK_SIZE_Z + z] = v; }
    inline void setBlockPosX(int y, int z, uint8_t v) { blocksPosX[y * CHUNK_SIZE_Z + z] = v; }
    inline void setBlockNegZ(int x, int y, uint8_t v) { blocksNegZ[x * CHUNK_SIZE_Y + y] = v; }
    inline void setBlockPosZ(int x, int y, uint8_t v) { blocksPosZ[x * CHUNK_SIZE_Y + y] = v; }

    inline void setLightNegX(int y, int z, uint8_t v) { lightNegX[y * CHUNK_SIZE_Z + z] = v; }
    inline void setLightPosX(int y, int z, uint8_t v) { lightPosX[y * CHUNK_SIZE_Z + z] = v; }
    inline void setLightNegZ(int x, int y, uint8_t v) { lightNegZ[x * CHUNK_SIZE_Y + y] = v; }
    inline void setLightPosZ(int x, int y, uint8_t v) { lightPosZ[x * CHUNK_SIZE_Y + y] = v; }
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


    static NeighborEdgeData cacheNeighborEdges(const ChunkCoord& coord);

     static void buildMeshData(Chunk &chunk, const NeighborEdgeData &neighbors);

     static void uploadPendingMeshes();

     static ChunkMeshTriple buildChunkMeshesInternal(const Chunk &chunk, const NeighborEdgeData &neighbors);

     static void uploadMeshToGPU(Chunk &chunk, const ChunkMeshTriple &meshData);

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

     static Model createModelFromBuffers(const ChunkMeshBuffers &buf, const ChunkCoord &coord);

     static void buildChunkMeshAsync(Chunk &chunk);

     static void uploadMeshToGPU(Chunk &chunk);

     static bool isFaceExposed(const Chunk &chunk, int x, int y, int z, int face, bool isTranslucent, const NeighborEdgeData &neighbors);

    static ChunkMeshTriple buildChunkMeshes(const Chunk &chunk);

     static void AddFaceWithAlpha(ChunkMeshBuffers &buf, const Vector3 &blockPos, int face, const BlockTextureDef &def,
                                  float lightLevel, Color tint, unsigned char alpha, const Chunk &chunk, const NeighborEdgeData &neighbors);

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

    // Cached frustum planes - updated once per frame
    static Plane cachedFrustumPlanes[6];
    static bool frustumPlanesValid;

    static void initWaterShader();
    static void updateWaterShader(float time);
    static void updateFrustumPlanes(const Camera3D& camera);
    static bool isBoxInCachedFrustum(const BoundingBox& box);

     static float getVertexLight(const Chunk &chunk, int bx, int by, int bz, int face, int vertex, const NeighborEdgeData &neighbors);

     static void initMeshThreadPool(int threads);

     static void shutdownMeshThreadPool();
};

// In Renderer.hpp - add these declarations



inline int numThreads = 4;

#endif //REFACTOREDCLONE_RENDERER_HPP