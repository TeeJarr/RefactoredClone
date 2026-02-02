//
// Created by Tristan on 1/30/26.
//

#include "../include/Renderer.hpp"

#include <ostream>
#include <ranges>
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <rlgl.h>
#include "Blocks.hpp"
#include "Settings.hpp"
#include <thread>
#include <unordered_set>
#include <string.h>
#include <iostream>
#include "Common.hpp"

#include "LightingSystem.hpp"
#include "MeshThreadPool.hpp"

constexpr float TILE_WIDTH = 160.0f;
constexpr float TILE_HEIGHT = 160.0f;
constexpr float ATLAS_WIDTH = 160.0f;
constexpr float ATLAS_HEIGHT = 64000.0f;

Texture2D Renderer::textureAtlas = {};
std::vector<std::thread> Renderer::workers;


NeighborEdgeData Renderer::cacheNeighborEdges(const ChunkCoord& coord) {
    NeighborEdgeData data;

    // std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

    // Cache -X neighbor's right edge
    auto it = ChunkHelper::activeChunks.find({coord.x - 1, coord.z});
    if (it != ChunkHelper::activeChunks.end() && it->second) {
        data.hasNegX = true;
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                data.setBlockNegX(y, z, it->second->blockPosition[CHUNK_SIZE_X - 1][y][z]);
                data.setLightNegX(y, z, it->second->getLightLevel(CHUNK_SIZE_X - 1, y, z));
            }
        }
    }

    // Cache +X neighbor's left edge
    it = ChunkHelper::activeChunks.find({coord.x + 1, coord.z});
    if (it != ChunkHelper::activeChunks.end() && it->second) {
        data.hasPosX = true;
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                data.setBlockPosX(y, z, it->second->blockPosition[0][y][z]);
                data.setLightPosX(y, z, it->second->getLightLevel(0, y, z));
            }
        }
    }

    // Cache -Z neighbor's back edge
    it = ChunkHelper::activeChunks.find({coord.x, coord.z - 1});
    if (it != ChunkHelper::activeChunks.end() && it->second) {
        data.hasNegZ = true;
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                data.setBlockNegZ(x, y, it->second->blockPosition[x][y][CHUNK_SIZE_Z - 1]);
                data.setLightNegZ(x, y, it->second->getLightLevel(x, y, CHUNK_SIZE_Z - 1));
            }
        }
    }

    // Cache +Z neighbor's front edge
    it = ChunkHelper::activeChunks.find({coord.x, coord.z + 1});
    if (it != ChunkHelper::activeChunks.end() && it->second) {
        data.hasPosZ = true;
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                data.setBlockPosZ(x, y, it->second->blockPosition[x][y][0]);
                data.setLightPosZ(x, y, it->second->getLightLevel(x, y, 0));
            }
        }
    }

    return data;
}


void Renderer::buildMeshData(Chunk& chunk, const NeighborEdgeData& neighbors) {
    auto meshData = std::make_unique<ChunkMeshTriple>();
    *meshData = buildChunkMeshesInternal(chunk, neighbors);

    chunk.pendingMeshData = std::move(meshData);
    chunk.meshReady = true;
}

// void Renderer::uploadPendingMeshes() {
//     printf("DEBUG: uploadPendingMeshes called\n");
//
//     std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//
//     int checked = 0;
//     int uploaded = 0;
//
//     for (auto& [coord, chunk] : ChunkHelper::activeChunks) {
//         checked++;
//         if (!chunk) continue;
//
//         printf("DEBUG: Chunk (%d,%d) meshReady=%d meshBuilding=%d\n",
//                coord.x, coord.z, chunk->meshReady.load(), chunk->meshBuilding.load());
//
//         if (!chunk->meshReady) continue;
//
//         printf("DEBUG: Uploading mesh for (%d, %d)\n", coord.x, coord.z);
//         uploadMeshToGPU(*chunk, *chunk->pendingMeshData);
//
//         chunk->pendingMeshData.reset();
//         chunk->meshReady = false;
//         chunk->loaded = true;
//         uploaded++;
//
//         if (uploaded >= 2) break;
//     }
//
//     printf("DEBUG: Checked %d chunks, uploaded %d\n", checked, uploaded);
// }
// This is your existing buildChunkMeshes, just renamed
ChunkMeshTriple Renderer::buildChunkMeshesInternal(const Chunk &chunk, const NeighborEdgeData& neighbors) {
    ChunkMeshTriple meshes;

    meshes.opaque.reserve(3000);
    meshes.translucent.reserve(500);
    meshes.water.reserve(500);

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                if (id == ID_AIR) continue;

                auto defIt = blockTextureDefs.find(id);
                if (defIt == blockTextureDefs.end()) continue;

                const BlockTextureDef &def = defIt->second;
                unsigned char alpha = getBlockAlpha(id);
                bool isTranslucent = isBlockTranslucent(id);

                ChunkMeshBuffers *buf;
                if (id == ID_WATER) {
                    buf = &meshes.water;
                } else if (isTranslucent) {
                    buf = &meshes.translucent;
                } else {
                    buf = &meshes.opaque;
                }

                for (int f = 0; f < 6; f++) {
                    if (!isFaceExposed(chunk, x, y, z, f, isTranslucent, neighbors)) continue;

                    Color faceTint = getBlockFaceTint(id, f);
                    AddFaceWithAlpha(*buf, {(float)x, (float)y, (float)z},
                                     f, def, FACE_LIGHT[f], faceTint, alpha, chunk, neighbors);
                }
            }
        }
    }

    return meshes;
}

void Renderer::uploadMeshToGPU(Chunk& chunk, const ChunkMeshTriple& meshData) {
    // Unload old models
    if (chunk.opaqueModel.meshCount > 0 && IsModelValid(chunk.opaqueModel)) {
        UnloadModel(chunk.opaqueModel);
        chunk.opaqueModel = {0};
    }
    if (chunk.translucentModel.meshCount > 0 && IsModelValid(chunk.translucentModel)) {
        UnloadModel(chunk.translucentModel);
        chunk.translucentModel = {0};
    }
    if (chunk.waterModel.meshCount > 0 && IsModelValid(chunk.waterModel)) {
        UnloadModel(chunk.waterModel);
        chunk.waterModel = {0};
    }

    // Upload opaque mesh
    if (!meshData.opaque.vertices.empty()) {
        chunk.opaqueModel = createModelFromBuffers(meshData.opaque, chunk.chunkCoords);
    }

    // Upload translucent mesh
    if (!meshData.translucent.vertices.empty()) {
        chunk.translucentModel = createModelFromBuffers(meshData.translucent, chunk.chunkCoords);
    }

    // Upload water mesh
    if (!meshData.water.vertices.empty()) {
        chunk.waterModel = createModelFromBuffers(meshData.water, chunk.chunkCoords);
    }
}

// Model Renderer::createModelFromBuffers(const ChunkMeshBuffers& buf, const ChunkCoord& coord) {
//     Mesh mesh = {0};
//
//     mesh.vertexCount = buf.vertices.size() / 3;
//     mesh.triangleCount = buf.indices.size() / 3;
//
//     // Allocate and copy vertex data
//     mesh.vertices = (float*)MemAlloc(buf.vertices.size() * sizeof(float));
//     memcpy(mesh.vertices, buf.vertices.data(), buf.vertices.size() * sizeof(float));
//
//     mesh.normals = (float*)MemAlloc(buf.normals.size() * sizeof(float));
//     memcpy(mesh.normals, buf.normals.data(), buf.normals.size() * sizeof(float));
//
//     mesh.texcoords = (float*)MemAlloc(buf.texcoords.size() * sizeof(float));
//     memcpy(mesh.texcoords, buf.texcoords.data(), buf.texcoords.size() * sizeof(float));
//
//     mesh.colors = (unsigned char*)MemAlloc(buf.colors.size() * sizeof(unsigned char));
//     memcpy(mesh.colors, buf.colors.data(), buf.colors.size() * sizeof(unsigned char));
//
//     mesh.indices = (unsigned short*)MemAlloc(buf.indices.size() * sizeof(unsigned short));
//     memcpy(mesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));
//
//     // Upload to GPU
//     UploadMesh(&mesh, false);
//
//     // Create model
//     Model model = LoadModelFromMesh(mesh);
//     model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textureAtlas;
//
//     // Position the model
//     model.transform = MatrixTranslate(
//         coord.x * CHUNK_SIZE_X,
//         0,
//         coord.z * CHUNK_SIZE_Z
//     );
//
//     return model;
// }

// void Renderer::buildChunkMeshAsync(Chunk& chunk) {
//     if (chunk.meshBuilding.exchange(true)) return;  // Already building
//
//     // Cache neighbors (need lock here since we're on worker thread)
//     NeighborEdgeData neighbors = cacheNeighborEdges(chunk.chunkCoords);
//
//     // Build mesh data (CPU only, no OpenGL)
//     auto meshData = std::make_unique<ChunkMeshTriple>();
//     *meshData = buildChunkMeshesInternal(chunk, neighbors);
//
//     // Store result
//     chunk.pendingMeshData = std::move(meshData);
//     chunk.meshBuilding = false;
//     chunk.meshReady = true;
// }
//
// // Call this every frame on main thread
// void Renderer::processChunkBuildQueue(const Camera3D &camera) {
//     constexpr int MAX_CHUNKS_PER_FRAME = 2;
//     int processed = 0;
//
//     while (processed < MAX_CHUNKS_PER_FRAME) {
//         std::unique_ptr<Chunk> chunk;
//         if (!ChunkHelper::chunkBuildQueue.try_pop(chunk)) {
//             printf("DEBUG: No chunks in queue\n");
//             break;
//         }
//         if (!chunk) {
//             printf("DEBUG: Chunk is null\n");
//             continue;
//         }
//         ChunkCoord playerChunk = getPlayerChunkCoord(camera);
//         int dx = abs(chunk->chunkCoords.x - playerChunk.x);
//         int dz = abs(chunk->chunkCoords.z - playerChunk.z);
//         if (dx > Settings::preLoadDistance || dz > Settings::preLoadDistance) {
//             ChunkHelper::chunkBuildQueue.push(std::move(chunk));
//             break;
//         }
//
//         ChunkCoord coord = chunk->chunkCoords;
//         printf("DEBUG: Processing chunk (%d, %d)\n", chunk->chunkCoords.x, chunk->chunkCoords.z);
//
//         // Calculate lighting (fast, do on main thread)
//         LightingSystem::calculateSkyLight(*chunk);
//         LightingSystem::calculateBlockLight(*chunk);
//
//         // Spread light from neighbors
//         {
//             std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//             auto& activeChunks = ChunkHelper::activeChunks;
//             if (activeChunks.count({coord.x - 1, coord.z})) {
//                 LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x - 1, coord.z}], 0);
//             }
//             if (activeChunks.count({coord.x + 1, coord.z})) {
//                 LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x + 1, coord.z}], 1);
//             }
//             if (activeChunks.count({coord.x, coord.z - 1})) {
//                 LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x, coord.z - 1}], 2);
//             }
//             if (activeChunks.count({coord.x, coord.z + 1})) {
//                 LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x, coord.z + 1}], 3);
//             }
//         }
//
//         // Add to active chunks first
//         replaceChunk(coord, std::move(chunk));
//
//         // Submit mesh building to thread pool
//         printf("DEBUG: Submitting to thread pool\n");
//         g_meshThreadPool.submit([coord]() {
//             printf("DEBUG: Thread pool task started for (%d, %d)\n", coord.x, coord.z);
//             std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//             auto it = ChunkHelper::activeChunks.find(coord);
//             if (it != ChunkHelper::activeChunks.end() && it->second) {
//                 Renderer::buildChunkMeshAsync(*it->second);
//             }
//         });
//
//         // Mark neighbors dirty
//         ChunkHelper::markChunkDirty({coord.x - 1, coord.z});
//         ChunkHelper::markChunkDirty({coord.x + 1, coord.z});
//         ChunkHelper::markChunkDirty({coord.x, coord.z - 1});
//         ChunkHelper::markChunkDirty({coord.x, coord.z + 1});
//
//         processed++;
//     }
// }
// Update isFaceExposed to handle translucent blocks properly

bool Renderer::isFaceExposed(const Chunk &chunk, int x, int y, int z, int face,
                              bool isTranslucent, const NeighborEdgeData& neighbors) {
    int nx = x + dx[face];
    int ny = y + dy[face];
    int nz = z + dz[face];

    if (ny < 0 || ny >= CHUNK_SIZE_Y) return true;

    int neighborId;

 if (nx < 0) {
        if (!neighbors.hasNegX) return true;  // CHANGE: Show face if no neighbor data
        neighborId = neighbors.getBlockNegX(ny, nz);
    } else if (nx >= CHUNK_SIZE_X) {
        if (!neighbors.hasPosX) return true;  // CHANGE: Show face if no neighbor data
        neighborId = neighbors.getBlockPosX(ny, nz);
    } else if (nz < 0) {
        if (!neighbors.hasNegZ) return true;  // CHANGE: Show face if no neighbor data
        neighborId = neighbors.getBlockNegZ(nx, ny);
    } else if (nz >= CHUNK_SIZE_Z) {
        if (!neighbors.hasPosZ) return true;  // CHANGE: Show face if no neighbor data
        neighborId = neighbors.getBlockPosZ(nx, ny);
    } else {
        neighborId = chunk.blockPosition[nx][ny][nz];
    }

    if (neighborId == ID_AIR) return true;

    if (isTranslucent) {
        int currentId = chunk.blockPosition[x][y][z];
        if (neighborId == currentId) return false;
        return true;
    }

    return isBlockTranslucent(static_cast<BlockIds>(neighborId));
}


// Modified AddFace that supports alpha
void Renderer::AddFaceWithAlpha(
    ChunkMeshBuffers &buf,
    const Vector3 &blockPos,
    int face,
    const BlockTextureDef &def,
    float lightLevel,
    Color tint,
    unsigned char alpha,
    const Chunk &chunk,
    const NeighborEdgeData &neighbors  // ADD THIS
) {
    static const Vector3 FACE_VERTS[6][4] = {
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, // Front (-Z)
        {{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}}, // Back (+Z)
        {{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}}, // Left (-X)
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}}, // Right (+X)
        {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}}, // Top (+Y)
        {{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}}  // Bottom (-Y)
    };

    static const Vector3 FACE_NORMALS[6] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };

    static const Vector2 FACE_UVS[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    int idx = def.faceTile[face];
    UVRect uv = GetAtlasUV(idx,
                           Renderer::textureAtlas.width,
                           Renderer::textureAtlas.height,
                           TILE_WIDTH,
                           TILE_HEIGHT);

    int indexOffset = buf.vertices.size() / 3;
    bool flipV = (face >= 0 && face <= 3);

    for (int v = 0; v < 4; v++) {
        Vector3 vert = Vector3Add(FACE_VERTS[face][v], blockPos);
        buf.vertices.push_back(vert.x);
        buf.vertices.push_back(vert.y);
        buf.vertices.push_back(vert.z);

        buf.normals.push_back(FACE_NORMALS[face].x);
        buf.normals.push_back(FACE_NORMALS[face].y);
        buf.normals.push_back(FACE_NORMALS[face].z);

        float u = uv.u0 + (uv.u1 - uv.u0) * FACE_UVS[v].x;
        float tv = uv.v0 + (uv.v1 - uv.v0) * (flipV ? 1.0f - FACE_UVS[v].y : FACE_UVS[v].y);
        buf.texcoords.push_back(u);
        buf.texcoords.push_back(tv);

        // Updated to use neighbors
        float vertexLight = getVertexLight(chunk,
            (int)blockPos.x, (int)blockPos.y, (int)blockPos.z, face, v, neighbors);

        float combinedLight = vertexLight * FACE_LIGHT[face];

        unsigned char r = (unsigned char)(tint.r * combinedLight);
        unsigned char g = (unsigned char)(tint.g * combinedLight);
        unsigned char b = (unsigned char)(tint.b * combinedLight);

        buf.colors.push_back(r);
        buf.colors.push_back(g);
        buf.colors.push_back(b);
        buf.colors.push_back(alpha);
    }

    buf.indices.push_back(indexOffset + 0);
    buf.indices.push_back(indexOffset + 2);
    buf.indices.push_back(indexOffset + 1);
    buf.indices.push_back(indexOffset + 0);
    buf.indices.push_back(indexOffset + 3);
    buf.indices.push_back(indexOffset + 2);
}
// Update buildChunkMeshes to use tints
ChunkMeshTriple Renderer::buildChunkMeshes(const Chunk &chunk) {
    ChunkMeshTriple meshes;

    // Cache neighbor data ONCE at the start
    NeighborEdgeData neighbors = cacheNeighborEdges(chunk.chunkCoords);

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                if (id == ID_AIR) continue;

                auto defIt = blockTextureDefs.find(id);
                if (defIt == blockTextureDefs.end()) continue;

                const BlockTextureDef &def = defIt->second;
                unsigned char alpha = getBlockAlpha(id);
                bool isTranslucent = isBlockTranslucent(id);

                ChunkMeshBuffers *buf;
                if (id == ID_WATER) {
                    buf = &meshes.water;
                } else if (isTranslucent) {
                    buf = &meshes.translucent;
                } else {
                    buf = &meshes.opaque;
                }

                for (int f = 0; f < 6; f++) {
                    if (!isFaceExposed(chunk, x, y, z, f, isTranslucent, neighbors)) continue;

                    Color faceTint = getBlockFaceTint(id, f);
                    AddFaceWithAlpha(*buf, {(float)x, (float)y, (float)z},
                                     f, def, FACE_LIGHT[f], faceTint, alpha, chunk, neighbors);
                }
            }
        }
    }

    return meshes;
}

void Renderer::buildChunkModel(const Chunk &chunk) {
    ChunkMeshTriple meshes = buildChunkMeshes(chunk);

    // Debug output
    std::println("Chunk ({}, {}) - opaque: {}, translucent: {}, water: {}",
                 chunk.chunkCoords.x, chunk.chunkCoords.z,
                 meshes.opaque.vertices.size() / 3,
                 meshes.translucent.vertices.size() / 3,
                 meshes.water.vertices.size() / 3);

    // Build opaque model
    if (chunk.opaqueModel.meshCount > 0 && IsModelValid(chunk.opaqueModel)) {
        UnloadModel(chunk.opaqueModel);
    }
    if (!meshes.opaque.vertices.empty()) {
        chunk.opaqueModel = buildModelFromBuffers(meshes.opaque);
    } else {
        chunk.opaqueModel = {0};
    }

    // Build translucent model
    if (chunk.translucentModel.meshCount > 0 && IsModelValid(chunk.translucentModel)) {
        UnloadModel(chunk.translucentModel);
    }
    if (!meshes.translucent.vertices.empty()) {
        chunk.translucentModel = buildModelFromBuffers(meshes.translucent);
    } else {
        chunk.translucentModel = {0};
    }

    // Build water model
    if (chunk.waterModel.meshCount > 0 && IsModelValid(chunk.waterModel)) {
        UnloadModel(chunk.waterModel);
    }
    if (!meshes.water.vertices.empty()) {
        chunk.waterModel = buildModelFromBuffers(meshes.water);
        std::println("Water model built with {} meshes", chunk.waterModel.meshCount);
    } else {
        chunk.waterModel = {0};
        std::println("No water vertices - model not built");
    }
}

void Renderer::drawChunkTranslucent(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
    if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
    if (!chunk->loaded) return;
    if (chunk->translucentModel.meshCount == 0) return;

    Vector3 worldPos = {
        (float) (chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float) (chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    // NO shader - just regular rendering for leaves/glass
    DrawModel(chunk->translucentModel, worldPos, 1.0f, WHITE);
}

void Renderer::drawChunkWater(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
    if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
    if (!chunk->loaded) return;
    if (chunk->waterModel.meshCount == 0) return;

    Vector3 worldPos = {
        (float) (chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float) (chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    // Apply water shader only to water
    chunk->waterModel.materials[0].shader = waterShader;

    DrawModel(chunk->waterModel, worldPos, 1.0f, WHITE);
}

Model Renderer::buildModelFromBuffers(ChunkMeshBuffers &buf) {
    if (buf.vertices.empty()) {
        return {0};
    }

    Mesh mesh = {0};
    mesh.vertexCount = buf.vertices.size() / 3;
    mesh.triangleCount = buf.indices.size() / 3;

    mesh.vertices = (float *) MemAlloc(buf.vertices.size() * sizeof(float));
    memcpy(mesh.vertices, buf.vertices.data(), buf.vertices.size() * sizeof(float));

    mesh.normals = (float *) MemAlloc(buf.normals.size() * sizeof(float));
    memcpy(mesh.normals, buf.normals.data(), buf.normals.size() * sizeof(float));

    mesh.texcoords = (float *) MemAlloc(buf.texcoords.size() * sizeof(float));
    memcpy(mesh.texcoords, buf.texcoords.data(), buf.texcoords.size() * sizeof(float));

    mesh.colors = (unsigned char *) MemAlloc(buf.colors.size());
    memcpy(mesh.colors, buf.colors.data(), buf.colors.size());

    mesh.indices = (unsigned short *) MemAlloc(buf.indices.size() * sizeof(unsigned short));
    memcpy(mesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));

    UploadMesh(&mesh, true);

    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textureAtlas;

    return model;
}

// Draw opaque parts
void Renderer::drawChunkOpaque(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
    if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
    if (!chunk->loaded) return;

    chunk->alpha += GetFrameTime() * 2.0f;
    if (chunk->alpha > 1.0f) chunk->alpha = 1.0f;

    Vector3 worldPos = {
        (float) (chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float) (chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    Color tint = WHITE;
    tint.a = (unsigned char) (chunk->alpha * 255);

    if (chunk->opaqueModel.meshCount > 0) {
        DrawModel(chunk->opaqueModel, worldPos, 1.0f, tint);
    }
}


// Main draw function that handles render order
void Renderer::drawAllChunks(const Camera3D &camera) {
    // First pass: draw all opaque geometry
    for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
        if (chunk) {
            drawChunkOpaque(chunk, camera);
        }
    }

    // Sort translucent chunks by distance from camera (back to front)
    std::vector<std::pair<float, ChunkCoord> > translucentChunks;
    std::vector<std::pair<float, ChunkCoord> > waterChunks;

    for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
        if (chunk && chunk->translucentModel.meshCount > 0 || chunk->waterModel.meshCount > 0) {
            Vector3 chunkCenter = {
                (float) (coord.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2),
                CHUNK_SIZE_Y / 2.0f,
                (float) (coord.z * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2)
            };
            float dist = Vector3DistanceSqr(camera.position, chunkCenter);
            translucentChunks.push_back({dist, coord});
        }
    }

    // Sort back to front (furthest first)
    std::sort(translucentChunks.begin(), translucentChunks.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    // Enable blending for translucent geometry
    rlDisableDepthMask(); // Don't write to depth buffer for translucent
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);

    // Second pass: draw translucent geometry back to front
    for (auto &[dist, coord]: translucentChunks) {
        auto it = ChunkHelper::activeChunks.find(coord);
        if (it != ChunkHelper::activeChunks.end() && it->second) {
            if (it->second->translucentModel.meshCount > 0) {
                drawChunkTranslucent(it->second, camera);
            }
            if (it->second->waterModel.meshCount > 0) {
                drawChunkWater(it->second, camera);
            }
        }
    }

    // Restore state
    rlEnableDepthMask();
    rlSetBlendMode(BLEND_ALPHA);
}



UVRect Renderer::GetTileUV(int tileIndex, int atlasWidth, int atlasHeight, int tileSize) {
    float u0 = 0.0f;
    float u1 = 1.0f; // full width
    float v0 = (float) (tileIndex * tileSize) / (float) atlasHeight;
    float v1 = (float) ((tileIndex + 1) * tileSize) / (float) atlasHeight;
    return {u0, v0, u1, v1};
}

bool Renderer::isFaceExposed(const Chunk &chunk, int x, int y, int z, int face) {
    int nx = x + dx[face];
    int ny = y + dy[face];
    int nz = z + dz[face];

    // Outside chunk = exposed
    if (nx < 0 || nx >= CHUNK_SIZE_X ||
        ny < 0 || ny >= CHUNK_SIZE_Y ||
        nz < 0 || nz >= CHUNK_SIZE_Z)
        return true;

    int currentId = chunk.blockPosition[x][y][z];
    int neighborId = chunk.blockPosition[nx][ny][nz];

    // Air neighbor = always exposed
    if (neighborId == ID_AIR) return true;

    // Current block is water
    if (currentId == ID_WATER) {
        // Water next to water = don't show face (except top)
        if (neighborId == ID_WATER) {
            return false;
        }
        // Water next to anything else = show face
        return true;
    }

    // Current block is translucent (leaves, glass)
    if (isBlockTranslucent(static_cast<BlockIds>(currentId))) {
        // Translucent next to same type = don't show
        if (currentId == neighborId) {
            return false;
        }
        // Translucent next to air or different block = show
        return true;
    }

    // Opaque block next to translucent = show face
    if (isBlockTranslucent(static_cast<BlockIds>(neighborId))) {
        return true;
    }

    // Opaque next to opaque = don't show
    return false;
}

Plane Renderer::normalizePlane(const Plane &plane) {
    float len = Vector3Length(plane.normal);
    return Plane(Vector3Scale(plane.normal, 1.0f / len), plane.distance / len);
}

bool Renderer::isBoxOnScreen(const BoundingBox &box, const Camera3D &camera) {
    Plane planes[6];
    GetCameraFrustumPlanes(camera, planes);
    return IsBoxInFrustum(box, planes);
}

Vector3 Renderer::WorldToCamera(const Vector3 &p, const Camera3D &cam) {
    Matrix view = GetCameraMatrix(cam);
    return Vector3Transform(p, view);
}

void Renderer::GetCameraFrustumPlanes(const Camera3D &cam, Plane planes[6]) {
    Matrix view = GetCameraMatrix(cam);
    float aspect = (float) GetScreenWidth() / (float) GetScreenHeight();
    Matrix proj = MatrixPerspective(cam.fovy * DEG2RAD, aspect, 0.01f, 1000.0f);

    Matrix clip = MatrixMultiply(view, proj);

    // Left plane
    planes[0].normal.x = clip.m3 + clip.m0;
    planes[0].normal.y = clip.m7 + clip.m4;
    planes[0].normal.z = clip.m11 + clip.m8;
    planes[0].distance = clip.m15 + clip.m12;

    // Right plane
    planes[1].normal.x = clip.m3 - clip.m0;
    planes[1].normal.y = clip.m7 - clip.m4;
    planes[1].normal.z = clip.m11 - clip.m8;
    planes[1].distance = clip.m15 - clip.m12;

    // Bottom plane
    planes[2].normal.x = clip.m3 + clip.m1;
    planes[2].normal.y = clip.m7 + clip.m5;
    planes[2].normal.z = clip.m11 + clip.m9;
    planes[2].distance = clip.m15 + clip.m13;

    // Top plane
    planes[3].normal.x = clip.m3 - clip.m1;
    planes[3].normal.y = clip.m7 - clip.m5;
    planes[3].normal.z = clip.m11 - clip.m9;
    planes[3].distance = clip.m15 - clip.m13;

    // Near plane
    planes[4].normal.x = clip.m3 + clip.m2;
    planes[4].normal.y = clip.m7 + clip.m6;
    planes[4].normal.z = clip.m11 + clip.m10;
    planes[4].distance = clip.m15 + clip.m14;

    // Far plane
    planes[5].normal.x = clip.m3 - clip.m2;
    planes[5].normal.y = clip.m7 - clip.m6;
    planes[5].normal.z = clip.m11 - clip.m10;
    planes[5].distance = clip.m15 - clip.m14;

    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        planes[i] = normalizePlane(planes[i]);
    }
}


bool Renderer::IsBoxInFrustum(const BoundingBox &box, const Plane planes[6]) {
    // Test all planes
    for (int i = 0; i < 6; i++) {
        const Plane &p = planes[i];

        // Check all 8 corners of the box
        bool allOutside = true;
        for (int x = 0; x <= 1; x++)
            for (int y = 0; y <= 1; y++)
                for (int z = 0; z <= 1; z++) {
                    Vector3 corner = {
                        x ? box.max.x : box.min.x,
                        y ? box.max.y : box.min.y,
                        z ? box.max.z : box.min.z
                    };

                    if (Vector3DotProduct(p.normal, corner) + p.distance >= 0) {
                        allOutside = false; // at least one corner in front
                        break;
                    }
                }

        if (allOutside) return false; // box completely outside this plane
    }

    return true; // box intersects frustum
}

void Renderer::checkActiveChunks(const Camera3D &camera) {
    ChunkCoord playerChunk = getPlayerChunkCoord(camera);

    // Collect all coords to check first
    std::vector<ChunkCoord> coordsToRequest;

    // Single lock for checking active chunks
    {
        std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
        std::lock_guard<std::mutex> lock2(ChunkHelper::chunkRequestSetMutex);

        for (int dx = -Settings::preLoadDistance; dx <= Settings::preLoadDistance; dx++) {
            for (int dz = -Settings::preLoadDistance; dz <= Settings::preLoadDistance; dz++) {
                ChunkCoord coord{playerChunk.x + dx, playerChunk.z + dz};

                if (ChunkHelper::activeChunks.contains(coord)) continue;
                if (ChunkHelper::chunkRequestSet.contains(coord)) continue;

                ChunkHelper::chunkRequestSet.insert(coord);
                coordsToRequest.push_back(coord);
            }
        }
    }

    // Single lock for pushing to queue
    if (!coordsToRequest.empty()) {
        std::lock_guard<std::mutex> lock(ChunkHelper::chunkRequestQueueMtx);
        for (const auto& coord : coordsToRequest) {
            ChunkHelper::chunkRequestQueue.push(coord);
        }
    }

}


#include <cassert>

void Renderer::unloadChunks(const Camera3D &camera) {
    ChunkCoord playerChunk{
        worldToChunk(camera.position.x, CHUNK_SIZE_X),
        worldToChunk(camera.position.z, CHUNK_SIZE_Z)
    };

    std::vector<ChunkCoord> toRemove;

    for (const auto &[coord, chunk]: ChunkHelper::activeChunks) {
        int dx = coord.x - playerChunk.x;
        int dz = coord.z - playerChunk.z;

        if (abs(dx) > Settings::unloadDistance ||
            abs(dz) > Settings::unloadDistance) {
            if (chunk && chunk->loaded) {
                toRemove.push_back(coord);
            }
        }
    }

    for (const auto &coord: toRemove) {
        auto it = ChunkHelper::activeChunks.find(coord);
        if (it == ChunkHelper::activeChunks.end()) continue;

        // Unload both models
        if (it->second->opaqueModel.meshCount > 0 && IsModelValid(it->second->opaqueModel)) {
            UnloadModel(it->second->opaqueModel);
        }
        if (it->second->translucentModel.meshCount > 0 && IsModelValid(it->second->translucentModel)) {
            UnloadModel(it->second->translucentModel);
        }
        if (it->second->waterModel.meshCount > 0 && IsModelValid(it->second->waterModel)) {
            UnloadModel(it->second->waterModel);
        }

        it->second->loaded = false;
        ChunkHelper::activeChunks.erase(it);
    }
}

void Renderer::shutdown() {
    ChunkHelper::workerRunning = false;
    ChunkHelper::chunkRequestQueue.notifyAll();

    for (auto &t: Renderer::workers) {
        if (t.joinable()) t.join();
    }
    Renderer::workers.clear();

    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
    for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
        if (!chunk) continue;

        if (chunk->opaqueModel.meshCount > 0 && IsModelValid(chunk->opaqueModel)) {
            UnloadModel(chunk->opaqueModel);
        }
        if (chunk->translucentModel.meshCount > 0 && IsModelValid(chunk->translucentModel)) {
            UnloadModel(chunk->translucentModel);
        }
    }
    ChunkHelper::activeChunks.clear();

    if (textureAtlas.id > 0 && IsTextureValid(textureAtlas)) {
        UnloadTexture(textureAtlas);
        textureAtlas.id = 0;
    }
}

int Renderer::worldToChunk(float v, int chunkSize) {
    int i = (int) floor(v);
    return (i >= 0) ? (i / chunkSize) : ((i - chunkSize + 1) / chunkSize);
}

ChunkCoord Renderer::getPlayerChunkCoord(const Camera3D &camera) {
    auto worldToChunk = [](float v, int chunkSize) {
        int i = (int) floor(v);
        return (i >= 0) ? (i / chunkSize) : ((i - chunkSize + 1) / chunkSize);
    };

    return {
        worldToChunk(camera.position.x, CHUNK_SIZE_X),
        worldToChunk(camera.position.z, CHUNK_SIZE_Z)
    };
}


// Helper to add a single cube face to the mesh buffer
void Renderer::AddFace(
    ChunkMeshBuffers &buf,
    const Vector3 &blockPos,
    int face, // 0-5: front, back, left, right, top, bottom
    const BlockTextureDef &def,
    float lightLevel, // per-face brightness (0-1)
    bool tintTop // true if top face should be tinted
) {
    static const Vector3 FACE_VERTS[6][4] = {
        {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, // Front (-Z)
        {{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}}, // Back (+Z)
        {{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}}, // Left (-X)
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}}, // Right (+X)
        {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}}, // Top (+Y)
        {{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}} // Bottom (-Y)
    };

    static const Vector3 FACE_NORMALS[6] = {
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}
    };

    static const Vector2 FACE_UVS[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    // Compute UVs based on atlas size
    int idx = def.faceTile[face];
    // std::println("Texture Width: {}\nTexture Height: {}\n", textureAtlas.width, textureAtlas.height);
    UVRect uv = GetAtlasUV(idx,
                           Renderer::textureAtlas.width,
                           Renderer::textureAtlas.height,
                           TILE_WIDTH,
                           TILE_HEIGHT);

    int indexOffset = buf.vertices.size() / 3;
    bool flipV = (face >= 0 && face <= 3); // sides: front/back/left/right
    for (int v = 0; v < 4; v++) {
        Vector3 vert = Vector3Add(FACE_VERTS[face][v], blockPos);
        buf.vertices.push_back(vert.x);
        buf.vertices.push_back(vert.y);
        buf.vertices.push_back(vert.z);

        buf.normals.push_back(FACE_NORMALS[face].x);
        buf.normals.push_back(FACE_NORMALS[face].y);
        buf.normals.push_back(FACE_NORMALS[face].z);

        float u = uv.u0 + (uv.u1 - uv.u0) * FACE_UVS[v].x;
        float tv = uv.v0 + (uv.v1 - uv.v0) * (flipV ? 1.0f - FACE_UVS[v].y : FACE_UVS[v].y);
        buf.texcoords.push_back(u);
        buf.texcoords.push_back(tv);

        // Colors for lighting
        unsigned char r, g, b;
        if (tintTop && face == 4) {
            // grass top tint
            r = (unsigned char) (lightLevel * 105.0f);
            g = (unsigned char) (lightLevel * 175.0f);
            b = (unsigned char) (lightLevel * 59.0f);
        } else {
            unsigned char l = (unsigned char) (lightLevel * 255.0f);
            r = g = b = l;
        }
        buf.colors.push_back(r);
        buf.colors.push_back(g);
        buf.colors.push_back(b);
        buf.colors.push_back(255);
    }

    // Indices for two triangles
    buf.indices.push_back(indexOffset + 0);
    buf.indices.push_back(indexOffset + 2);
    buf.indices.push_back(indexOffset + 1);
    buf.indices.push_back(indexOffset + 0);
    buf.indices.push_back(indexOffset + 3);
    buf.indices.push_back(indexOffset + 2);
}

// Build a chunk model using the above AddFace
// Model Renderer::buildChunkModel(const Chunk &chunk) {
//     ChunkMeshBuffers buf;
//
//     for (int x = 0; x < CHUNK_SIZE_X; x++)
//         for (int y = 0; y < CHUNK_SIZE_Y; y++)
//             for (int z = 0; z < CHUNK_SIZE_Z; z++) {
//                 BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
//                 if (id == ID_AIR) continue;
//
//                 auto def = blockTextureDefs.at(id);
//
//                 for (int f = 0; f < 6; f++) {
//                     if (!Renderer::isFaceExposed(chunk, x, y, z, f)) continue;
//
//                     // int tile = def.faceTile[f];   // Correct tile per face
//                     bool tintTop = def.tintTop;
//
//                     AddFace(buf, (Vector3){(float) x, (float) y, (float) z}, f, def, FACE_LIGHT[f], tintTop);
//                 }
//             }
//
//     // Upload mesh
//     Mesh mesh = {0};
//     mesh.vertexCount = buf.vertices.size() / 3;
//     mesh.triangleCount = buf.indices.size() / 3;
//
//     mesh.vertices = (float *) MemAlloc(buf.vertices.size() * sizeof(float));
//     memcpy(mesh.vertices, buf.vertices.data(), buf.vertices.size() * sizeof(float));
//
//     mesh.normals = (float *) MemAlloc(buf.normals.size() * sizeof(float));
//     memcpy(mesh.normals, buf.normals.data(), buf.normals.size() * sizeof(float));
//
//     mesh.texcoords = (float *) MemAlloc(buf.texcoords.size() * sizeof(float));
//     memcpy(mesh.texcoords, buf.texcoords.data(), buf.texcoords.size() * sizeof(float));
//
//     mesh.colors = (unsigned char *) MemAlloc(buf.colors.size());
//     memcpy(mesh.colors, buf.colors.data(), buf.colors.size());
//
//     mesh.indices = (unsigned short *) MemAlloc(buf.indices.size() * sizeof(unsigned short));
//     memcpy(mesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));
//
//
//     UploadMesh(&mesh, true);
//
//     Model model = LoadModelFromMesh(mesh);
//     model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textureAtlas;
//
//     return model;
// }

// Compute the UV rect of a tile in a scalable atlas
UVRect Renderer::GetAtlasUV(int tileIndex, int atlasWidth, int atlasHeight, int tileWidth, int tileHeight) {
    int tilesPerRow = atlasWidth / tileWidth;
    int tilesPerCol = atlasHeight / tileHeight;
    int totalTiles = tilesPerRow * tilesPerCol;

    if (tileIndex >= totalTiles) tileIndex = totalTiles - 1;

    int tileX = 0;
    int tileY = tileIndex / tilesPerRow;

    float u0 = (float) (tileX * tileWidth) / atlasWidth;
    float u1 = (float) ((tileX + 1) * tileWidth) / atlasWidth;
    float v0 = (float) (tileY * tileHeight) / atlasHeight;
    float v1 = (float) ((tileY + 1) * tileHeight) / atlasHeight;

    return {u0, v0, u1, v1};
}

void Renderer::replaceChunk(const ChunkCoord &coord, std::unique_ptr<Chunk> newChunk) {
    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

try {
    auto it = ChunkHelper::activeChunks.find(coord);
    if (it != ChunkHelper::activeChunks.end() && it->second) {
        if (it->second->opaqueModel.meshCount > 0 && IsModelValid(it->second->opaqueModel)) {
            UnloadModel(it->second->opaqueModel);
        }
        if (it->second->translucentModel.meshCount > 0 && IsModelValid(it->second->translucentModel)) {
            UnloadModel(it->second->translucentModel);
        }
        if (it->second->waterModel.meshCount > 0 && IsModelValid(it->second->waterModel)) {
            UnloadModel(it->second->waterModel);
        }
    }
} catch (...) {
    std::cerr << "Map access crashed!" << std::endl;
}

    ChunkHelper::activeChunks[coord] = std::move(newChunk);
}

void Renderer::drawCrosshair() {
    float screenMiddleY = (float) GetScreenHeight() / 2.0f;
    float screenMiddleX = (float) GetScreenWidth() / 2.0f;

    float crosshairWidth = 10.0f;

    float chVertDraw = screenMiddleX - (crosshairWidth / 2.0f);
    float chHorzDraw = screenMiddleY - (crosshairWidth / 2.0f);

    DrawRectangleV({chVertDraw, chHorzDraw}, {crosshairWidth, crosshairWidth}, WHITE);
}

// In Chunk.cpp
// void Renderer::rebuildDirtyChunks() {
//     std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//     int rebuiltThisFrame = 0;
//     constexpr int MAX_REBUILDS_PER_FRAME = 2;  // Limit rebuilds
//
//     for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
//         if (!chunk || !chunk->dirty) continue;
//
//         // Recalculate this chunk's lighting
//         LightingSystem::calculateSkyLight(*chunk);
//         LightingSystem::calculateBlockLight(*chunk);
//
//         // Spread light FROM all existing neighbors INTO this chunk
//         if (ChunkHelper::activeChunks.count({coord.x - 1, coord.z})) {
//             auto &neighbor = ChunkHelper::activeChunks.at({coord.x - 1, coord.z});
//             LightingSystem::spreadLightFromNeighbor(*chunk, *neighbor, 0);
//         }
//         if (ChunkHelper::activeChunks.count({coord.x + 1, coord.z})) {
//             auto &neighbor = ChunkHelper::activeChunks.at({coord.x + 1, coord.z});
//             LightingSystem::spreadLightFromNeighbor(*chunk, *neighbor, 1);
//         }
//         if (ChunkHelper::activeChunks.count({coord.x, coord.z - 1})) {
//             auto &neighbor = ChunkHelper::activeChunks.at({coord.x, coord.z - 1});
//             LightingSystem::spreadLightFromNeighbor(*chunk, *neighbor, 2);
//         }
//         if (ChunkHelper::activeChunks.count({coord.x, coord.z + 1})) {
//             auto &neighbor = ChunkHelper::activeChunks.at({coord.x, coord.z + 1});
//             LightingSystem::spreadLightFromNeighbor(*chunk, *neighbor, 3);
//         }
//
//         // Unload old models
//         if (chunk->opaqueModel.meshCount > 0 && IsModelValid(chunk->opaqueModel)) {
//             UnloadModel(chunk->opaqueModel);
//             chunk->opaqueModel = {0};
//         }
//         if (chunk->translucentModel.meshCount > 0 && IsModelValid(chunk->translucentModel)) {
//             UnloadModel(chunk->translucentModel);
//             chunk->translucentModel = {0};
//         }
//
//         // Rebuild both models
//         buildChunkModel(*chunk);
//
//         chunk->dirty = false;
//         chunk->loaded = true;
//
//         rebuiltThisFrame++;
//     }
// }


Shader Renderer::waterShader = {0};
int Renderer::waterTimeLoc = 0;

void Renderer::initWaterShader() {
    const char *vsCode = R"(
        #version 330
        in vec3 vertexPosition;
        in vec2 vertexTexCoord;
        in vec4 vertexColor;
        out vec2 fragTexCoord;
        out vec4 fragColor;
        uniform mat4 mvp;
        void main() {
            fragTexCoord = vertexTexCoord;
            fragColor = vertexColor;
            gl_Position = mvp * vec4(vertexPosition, 1.0);
        }
    )";

    const char *fsCode = R"(
        #version 330
        in vec2 fragTexCoord;
        in vec4 fragColor;
        out vec4 finalColor;
        uniform sampler2D texture0;
        uniform float waterTime;

        void main() {
            float tileHeight = 0.025;
            int frameCount = 31;

            int frame = int(mod(waterTime * 10.0, float(frameCount)));

            vec2 animUV = fragTexCoord;
            animUV.y += float(frame) * tileHeight;

            vec4 texColor = texture(texture0, animUV);

            // Preserve vertex color alpha (water transparency)
            finalColor = vec4(texColor.rgb * fragColor.rgb, fragColor.a);
        }
    )";

    waterShader = LoadShaderFromMemory(vsCode, fsCode);
    waterTimeLoc = GetShaderLocation(waterShader, "waterTime");
}

void Renderer::updateWaterShader(float time) {
    SetShaderValue(waterShader, waterTimeLoc, &time, SHADER_UNIFORM_FLOAT);
}

// Calculate smooth lighting for a vertex
float Renderer::getVertexLight(const Chunk& chunk, int bx, int by, int bz, int face,
                                int vertex, const NeighborEdgeData& neighbors) {
    static const int fdx[6] = { 0, 0, -1, 1, 0, 0 };
    static const int fdy[6] = { 0, 0, 0, 0, 1, -1 };
    static const int fdz[6] = { -1, 1, 0, 0, 0, 0 };

    int lx = bx + fdx[face];
    int ly = by + fdy[face];
    int lz = bz + fdz[face];

    if (ly < 0) return 0.15f;
    if (ly >= CHUNK_SIZE_Y) return 1.0f;

    int lightLevel;
    bool usedNeighbor = false;

    if (lx < 0) {
        usedNeighbor = true;
        lightLevel = neighbors.hasNegX ? neighbors.getLightNegX(ly, lz) : 15;
    } else if (lx >= CHUNK_SIZE_X) {
        usedNeighbor = true;
        lightLevel = neighbors.hasPosX ? neighbors.getLightPosX(ly, lz) : 15;
    } else if (lz < 0) {
        usedNeighbor = true;
        lightLevel = neighbors.hasNegZ ? neighbors.getLightNegZ(lx, ly) : 15;
    } else if (lz >= CHUNK_SIZE_Z) {
        usedNeighbor = true;
        lightLevel = neighbors.hasPosZ ? neighbors.getLightPosZ(lx, ly) : 15;
    } else {
        lightLevel = chunk.getLightLevel(lx, ly, lz);
    }

    // DEBUG: Log edge cases
    static int debugCount = 0;
    if (usedNeighbor && debugCount++ < 50) {
        printf("DEBUG getVertexLight: block(%d,%d,%d) face=%d sample(%d,%d,%d) light=%d hasNeighbor=%s\n",
               bx, by, bz, face, lx, ly, lz, lightLevel,
               (lx < 0 ? (neighbors.hasNegX ? "YES" : "NO") :
                lx >= CHUNK_SIZE_X ? (neighbors.hasPosX ? "YES" : "NO") :
                lz < 0 ? (neighbors.hasNegZ ? "YES" : "NO") :
                (neighbors.hasPosZ ? "YES" : "NO")));
    }

    float minLight = 0.15f;
    return minLight + 0.85f * (lightLevel / 15.0f);
}

// Initialize mesh thread pool (call in Engine constructor after initChunkWorkers)
void Renderer::initMeshThreadPool(int threads) {
    g_meshThreadPool = std::make_unique<MeshThreadPool>(threads);
}

void Renderer::shutdownMeshThreadPool() {
    if (g_meshThreadPool) {
        g_meshThreadPool->shutdown();
        g_meshThreadPool.reset();
    }
}

// Build mesh data on worker thread (no OpenGL calls)
void Renderer::buildChunkMeshAsync(Chunk& chunk) {
    if (chunk.meshBuilding.exchange(true)) return;

    // Get neighbor data while holding the lock
    NeighborEdgeData neighbors = cacheNeighborEdges(chunk.chunkCoords);

    // Check if we have all neighbors - if not, faces at edges might be wrong
    printf("DEBUG: Building mesh for (%d,%d) - neighbors: -X=%d +X=%d -Z=%d +Z=%d\n",
           chunk.chunkCoords.x, chunk.chunkCoords.z,
           neighbors.hasNegX, neighbors.hasPosX, neighbors.hasNegZ, neighbors.hasPosZ);

    auto meshData = std::make_unique<ChunkMeshTriple>();
    *meshData = buildChunkMeshesInternal(chunk, neighbors);

    chunk.pendingMeshData = std::move(meshData);
    chunk.meshBuilding = false;
    chunk.meshReady = true;
}
// Upload mesh to GPU (main thread only)
 void Renderer::uploadMeshToGPU(Chunk& chunk) {
    if (!chunk.pendingMeshData) return;

    // Unload old models
    if (chunk.opaqueModel.meshCount > 0 && IsModelValid(chunk.opaqueModel)) {
        UnloadModel(chunk.opaqueModel);
        chunk.opaqueModel = {0};
    }
    if (chunk.translucentModel.meshCount > 0 && IsModelValid(chunk.translucentModel)) {
        UnloadModel(chunk.translucentModel);
        chunk.translucentModel = {0};
    }
    if (chunk.waterModel.meshCount > 0 && IsModelValid(chunk.waterModel)) {
        UnloadModel(chunk.waterModel);
        chunk.waterModel = {0};
    }

    const ChunkMeshTriple& meshData = *chunk.pendingMeshData;

    // Upload opaque mesh
    if (!meshData.opaque.vertices.empty()) {
        chunk.opaqueModel = createModelFromBuffers(meshData.opaque, chunk.chunkCoords);
    }

    // Upload translucent mesh
    if (!meshData.translucent.vertices.empty()) {
        chunk.translucentModel = createModelFromBuffers(meshData.translucent, chunk.chunkCoords);
    }

    // Upload water mesh
    if (!meshData.water.vertices.empty()) {
        chunk.waterModel = createModelFromBuffers(meshData.water, chunk.chunkCoords);
    }

    chunk.pendingMeshData.reset();
    chunk.meshReady = false;
    chunk.loaded = true;
}

Model Renderer::createModelFromBuffers(const ChunkMeshBuffers& buf, const ChunkCoord& coord) {
    Mesh mesh = {0};

    mesh.vertexCount = buf.vertices.size() / 3;
    mesh.triangleCount = buf.indices.size() / 3;

    mesh.vertices = (float*)MemAlloc(buf.vertices.size() * sizeof(float));
    memcpy(mesh.vertices, buf.vertices.data(), buf.vertices.size() * sizeof(float));

    mesh.normals = (float*)MemAlloc(buf.normals.size() * sizeof(float));
    memcpy(mesh.normals, buf.normals.data(), buf.normals.size() * sizeof(float));

    mesh.texcoords = (float*)MemAlloc(buf.texcoords.size() * sizeof(float));
    memcpy(mesh.texcoords, buf.texcoords.data(), buf.texcoords.size() * sizeof(float));

    mesh.colors = (unsigned char*)MemAlloc(buf.colors.size() * sizeof(unsigned char));
    memcpy(mesh.colors, buf.colors.data(), buf.colors.size() * sizeof(unsigned char));

    mesh.indices = (unsigned short*)MemAlloc(buf.indices.size() * sizeof(unsigned short));
    memcpy(mesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));

    UploadMesh(&mesh, false);

    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textureAtlas;

    return model;
}

// Process chunks from generation queue
void Renderer::initChunkWorkers(int threadCount) {
    ChunkHelper::workerRunning = true;
    for (int i = 0; i < threadCount; i++) {
        Renderer::workers.emplace_back([]() {
            while (ChunkHelper::workerRunning) {
                ChunkCoord coord;
                try {
                    coord = ChunkHelper::chunkRequestQueue.wait_pop(ChunkHelper::workerRunning);
                } catch (...) {
                    continue;
                }
                if (!ChunkHelper::workerRunning) break;

                auto chunk = ChunkHelper::generateChunkAsync({(float)coord.x, 0.0f, (float)coord.z});

                // Calculate lighting on worker thread (not main thread!)
                LightingSystem::calculateSkyLight(*chunk);
                LightingSystem::calculateBlockLight(*chunk);

                ChunkHelper::chunkBuildQueue.push(std::move(chunk));
            }
        });
    }
}

void Renderer::processChunkBuildQueue(const Camera3D &camera) {
    constexpr int MAX_CHUNKS_PER_FRAME = 2;
    int processed = 0;

    ChunkCoord playerChunk = getPlayerChunkCoord(camera);

    // Collect chunks from queue
    std::vector<std::unique_ptr<Chunk>> pendingChunks;
    {
        std::unique_ptr<Chunk> chunk;
        while (ChunkHelper::chunkBuildQueue.try_pop(chunk)) {
            if (chunk) {
                int dx = abs(chunk->chunkCoords.x - playerChunk.x);
                int dz = abs(chunk->chunkCoords.z - playerChunk.z);

                // Only keep chunks within range
                if (dx <= Settings::preLoadDistance && dz <= Settings::preLoadDistance) {
                    pendingChunks.push_back(std::move(chunk));
                }
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(pendingChunks.begin(), pendingChunks.end(),
        [&playerChunk](const std::unique_ptr<Chunk>& a, const std::unique_ptr<Chunk>& b) {
            int distA = abs(a->chunkCoords.x - playerChunk.x) + abs(a->chunkCoords.z - playerChunk.z);
            int distB = abs(b->chunkCoords.x - playerChunk.x) + abs(b->chunkCoords.z - playerChunk.z);
            return distA < distB;
        });

    // Process closest chunks first
    for (auto& chunk : pendingChunks) {
        if (processed >= MAX_CHUNKS_PER_FRAME) {
            // Put remaining chunks back in queue
            ChunkHelper::chunkBuildQueue.push(std::move(chunk));
            continue;
        }

        ChunkCoord coord = chunk->chunkCoords;

        // Spread light from neighbors
        {
            std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
            auto& activeChunks = ChunkHelper::activeChunks;
            if (activeChunks.count({coord.x - 1, coord.z})) {
                LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x - 1, coord.z}], 0);
            }
            if (activeChunks.count({coord.x + 1, coord.z})) {
                LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x + 1, coord.z}], 1);
            }
            if (activeChunks.count({coord.x, coord.z - 1})) {
                LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x, coord.z - 1}], 2);
            }
            if (activeChunks.count({coord.x, coord.z + 1})) {
                LightingSystem::spreadLightFromNeighbor(*chunk, *activeChunks[{coord.x, coord.z + 1}], 3);
            }
        }

        chunk->loaded = false;
        chunk->dirty = false;
        chunk->meshReady = false;
        chunk->meshBuilding = false;

        replaceChunk(coord, std::move(chunk));

        // Submit mesh building to thread pool
        g_meshThreadPool->submit([coord]() {
            std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
            auto it = ChunkHelper::activeChunks.find(coord);
            if (it != ChunkHelper::activeChunks.end() && it->second) {
                Renderer::buildChunkMeshAsync(*it->second);
            }
        });

        // Mark neighbors dirty
        ChunkHelper::markChunkDirty(coord);
        ChunkHelper::markChunkDirty({coord.x - 1, coord.z});
        ChunkHelper::markChunkDirty({coord.x + 1, coord.z});
        ChunkHelper::markChunkDirty({coord.x, coord.z - 1});
        ChunkHelper::markChunkDirty({coord.x, coord.z + 1});

        processed++;
    }
}

// Upload ready meshes to GPU (call every frame on main thread)
void Renderer::uploadPendingMeshes() {
    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

    int uploaded = 0;
    constexpr int MAX_UPLOADS_PER_FRAME = 4;

    for (auto& [coord, chunk] : ChunkHelper::activeChunks) {
        if (!chunk) continue;
        if (!chunk->meshReady.load()) continue;
        if (uploaded >= MAX_UPLOADS_PER_FRAME) break;

        uploadMeshToGPU(*chunk);
        uploaded++;
    }
}

// Rebuild dirty chunks (also threaded)
void Renderer::rebuildDirtyChunks() {
    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

    int submitted = 0;
    constexpr int MAX_SUBMITS_PER_FRAME = 1;  // Only 1!

    for (auto &[coord, chunk] : ChunkHelper::activeChunks) {
        if (!chunk || !chunk->dirty) continue;
        if (chunk->meshBuilding.load()) continue;
        if (submitted >= MAX_SUBMITS_PER_FRAME) break;

        chunk->dirty = false;

        // Submit to thread pool - do lighting AND mesh building on worker
        ChunkCoord c = coord;
        g_meshThreadPool->submit([c]() {
            std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
            auto it = ChunkHelper::activeChunks.find(c);
            if (it == ChunkHelper::activeChunks.end() || !it->second) return;

            Chunk& chunk = *it->second;

            // Recalculate lighting on worker thread
            LightingSystem::calculateSkyLight(chunk);
            LightingSystem::calculateBlockLight(chunk);

            // Spread from neighbors
            auto& activeChunks = ChunkHelper::activeChunks;
            if (activeChunks.count({c.x - 1, c.z})) {
                LightingSystem::spreadLightFromNeighbor(chunk, *activeChunks[{c.x - 1, c.z}], 0);
            }
            if (activeChunks.count({c.x + 1, c.z})) {
                LightingSystem::spreadLightFromNeighbor(chunk, *activeChunks[{c.x + 1, c.z}], 1);
            }
            if (activeChunks.count({c.x, c.z - 1})) {
                LightingSystem::spreadLightFromNeighbor(chunk, *activeChunks[{c.x, c.z - 1}], 2);
            }
            if (activeChunks.count({c.x, c.z + 1})) {
                LightingSystem::spreadLightFromNeighbor(chunk, *activeChunks[{c.x, c.z + 1}], 3);
            }

            // Build mesh
            Renderer::buildChunkMeshAsync(chunk);
        });

        submitted++;
    }
}