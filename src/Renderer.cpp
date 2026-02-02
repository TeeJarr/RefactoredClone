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

constexpr float TILE_WIDTH = 160.0f;
constexpr float TILE_HEIGHT = 160.0f;
constexpr float ATLAS_WIDTH = 160.0f;
constexpr float ATLAS_HEIGHT = 64000.0f;

Texture2D Renderer::textureAtlas = {};
std::vector<std::thread> Renderer::workers;

// Update isFaceExposed to handle translucent blocks properly
bool Renderer::isFaceExposed(const Chunk &chunk, int x, int y, int z, int face, bool isTranslucent) {
    int nx = x + dx[face];
    int ny = y + dy[face];
    int nz = z + dz[face];

    if (nx < 0 || nx >= CHUNK_SIZE_X ||
        ny < 0 || ny >= CHUNK_SIZE_Y ||
        nz < 0 || nz >= CHUNK_SIZE_Z)
        return true;

    int neighborId = chunk.blockPosition[nx][ny][nz];

    if (neighborId == ID_AIR) return true;

    // If current block is translucent
    if (isTranslucent) {
        // Don't render face between same translucent blocks (e.g., water-water)
        int currentId = chunk.blockPosition[x][y][z];
        if (neighborId == currentId) return false;
        // Render face if neighbor is different
        return true;
    }

    // Opaque block: show face if neighbor is translucent or air
    return isBlockTranslucent(neighborId);
}

// Build separate meshes for opaque and translucent
// ChunkMeshPair Renderer::buildChunkMeshes(const Chunk &chunk) {
//     ChunkMeshPair pair;
//
//     for (int x = 0; x < CHUNK_SIZE_X; x++) {
//         for (int y = 0; y < CHUNK_SIZE_Y; y++) {
//             for (int z = 0; z < CHUNK_SIZE_Z; z++) {
//                 BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
//                 if (id == ID_AIR) continue;
//
//                 auto def = blockTextureDefs.at(id);
//                 bool translucent = isBlockTranslucent(id);
//                 unsigned char alpha = getBlockAlpha(id);
//
//                 // Choose which buffer to add to
//                 ChunkMeshBuffers &buf = translucent ? pair.translucent : pair.opaque;
//
//                 for (int f = 0; f < 6; f++) {
//                     if (!isFaceExposed(chunk, x, y, z, f, translucent)) continue;
//
//                     AddFaceWithAlpha(buf,
//                         (Vector3){(float)x, (float)y, (float)z},
//                         f, def, FACE_LIGHT[f], def.tint, alpha);
//                 }
//             }
//         }
//     }
//
//     return pair;
// }

// Modified AddFace that supports alpha
void Renderer::AddFaceWithAlpha(
    ChunkMeshBuffers &buf,
    const Vector3 &blockPos,
    int face,
    const BlockTextureDef &def,
    float lightLevel,
    Color tint,
    unsigned char alpha
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

        // Apply tint and lighting
        unsigned char r = (unsigned char)(tint.r * lightLevel);
        unsigned char g = (unsigned char)(tint.g * lightLevel);
        unsigned char b = (unsigned char)(tint.b * lightLevel);

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

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                if (id == ID_AIR) continue;

                auto defIt = blockTextureDefs.find(id);
                if (defIt == blockTextureDefs.end()) continue;

                const BlockTextureDef& def = defIt->second;
                unsigned char alpha = getBlockAlpha(id);

                // Choose which buffer
                ChunkMeshBuffers* buf;
                if (id == ID_WATER) {
                    buf = &meshes.water;
                } else if (isBlockTranslucent(id)) {
                    buf = &meshes.translucent;
                } else {
                    buf = &meshes.opaque;
                }

                for (int f = 0; f < 6; f++) {
                    if (!isFaceExposed(chunk, x, y, z, f)) continue;

                    Color faceTint = getBlockFaceTint(id, f);

                    AddFaceWithAlpha(*buf,
                        (Vector3){(float)x, (float)y, (float)z},
                        f, def, FACE_LIGHT[f], faceTint, alpha);
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
        (float)(chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float)(chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    // NO shader - just regular rendering for leaves/glass
    DrawModel(chunk->translucentModel, worldPos, 1.0f, WHITE);
}

void Renderer::drawChunkWater(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
    if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
    if (!chunk->loaded) return;
    if (chunk->waterModel.meshCount == 0) return;

    Vector3 worldPos = {
        (float)(chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float)(chunk->chunkCoords.z * CHUNK_SIZE_Z)
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

    mesh.vertices = (float *)MemAlloc(buf.vertices.size() * sizeof(float));
    memcpy(mesh.vertices, buf.vertices.data(), buf.vertices.size() * sizeof(float));

    mesh.normals = (float *)MemAlloc(buf.normals.size() * sizeof(float));
    memcpy(mesh.normals, buf.normals.data(), buf.normals.size() * sizeof(float));

    mesh.texcoords = (float *)MemAlloc(buf.texcoords.size() * sizeof(float));
    memcpy(mesh.texcoords, buf.texcoords.data(), buf.texcoords.size() * sizeof(float));

    mesh.colors = (unsigned char *)MemAlloc(buf.colors.size());
    memcpy(mesh.colors, buf.colors.data(), buf.colors.size());

    mesh.indices = (unsigned short *)MemAlloc(buf.indices.size() * sizeof(unsigned short));
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
        (float)(chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float)(chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    Color tint = WHITE;
    tint.a = (unsigned char)(chunk->alpha * 255);

    if (chunk->opaqueModel.meshCount > 0) {
        DrawModel(chunk->opaqueModel, worldPos, 1.0f, tint);
    }
}


// Main draw function that handles render order
void Renderer::drawAllChunks(const Camera3D &camera) {
    // First pass: draw all opaque geometry
    for (auto &[coord, chunk] : ChunkHelper::activeChunks) {
        if (chunk) {
            drawChunkOpaque(chunk, camera);
        }
    }

    // Sort translucent chunks by distance from camera (back to front)
    std::vector<std::pair<float, ChunkCoord>> translucentChunks;
    std::vector<std::pair<float, ChunkCoord>> waterChunks;

    for (auto &[coord, chunk] : ChunkHelper::activeChunks) {
        if (chunk && chunk->translucentModel.meshCount > 0 || chunk->waterModel.meshCount > 0) {
            Vector3 chunkCenter = {
                (float)(coord.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2),
                CHUNK_SIZE_Y / 2.0f,
                (float)(coord.z * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2)
            };
            float dist = Vector3DistanceSqr(camera.position, chunkCenter);
            translucentChunks.push_back({dist, coord});
        }
    }

    // Sort back to front (furthest first)
    std::sort(translucentChunks.begin(), translucentChunks.end(),
        [](const auto &a, const auto &b) { return a.first > b.first; });

    // Enable blending for translucent geometry
    rlDisableDepthMask();  // Don't write to depth buffer for translucent
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);

    // Second pass: draw translucent geometry back to front
    for (auto &[dist, coord] : translucentChunks) {
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


// void Renderer::drawChunk(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
//     if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
//     if (!chunk->loaded) return;
//
//
//     chunk->alpha += GetFrameTime() * 2.0f; // fade-in over ~0.5 seconds
//     if (chunk->alpha > 1.0f) chunk->alpha = 1.0f;
//
//     Vector3 worldPos = {
//         (float) (chunk->chunkCoords.x * CHUNK_SIZE_X),
//         0.0f,
//         (float) (chunk->chunkCoords.z * CHUNK_SIZE_Z)
//     };
//
//     Color tint = WHITE;
//     tint.a = (unsigned char) (chunk->alpha * 255);
//
//     // std::println("Drawing chunk: {}, {}", chunk->chunkCoords.x, chunk->chunkCoords.z);
//     DrawModel(chunk->model, worldPos, 1.0f, tint);
// }

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

    for (int dx = -Settings::preLoadDistance; dx <= Settings::preLoadDistance; dx++) {
        for (int dz = -Settings::preLoadDistance; dz <= Settings::preLoadDistance; dz++) {
            ChunkCoord coord{
                playerChunk.x + dx,
                playerChunk.z + dz
            };

            {
                std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
                if (ChunkHelper::activeChunks.contains(coord))
                    continue;
            }

            {
                std::lock_guard<std::mutex> lock(ChunkHelper::chunkRequestSetMutex);
                if (ChunkHelper::chunkRequestSet.contains(coord))
                    continue;

                ChunkHelper::chunkRequestSet.insert(coord);
            }

            {
                std::lock_guard<std::mutex> lock(ChunkHelper::chunkRequestQueueMtx);
                ChunkHelper::chunkRequestQueue.push(coord);
            }
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

    for (const auto &[coord, chunk] : ChunkHelper::activeChunks) {
        int dx = coord.x - playerChunk.x;
        int dz = coord.z - playerChunk.z;

        if (abs(dx) > Settings::unloadDistance ||
            abs(dz) > Settings::unloadDistance) {
            if (chunk && chunk->loaded) {
                toRemove.push_back(coord);
            }
        }
    }

    for (const auto &coord : toRemove) {
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

    for (auto &t : Renderer::workers) {
        if (t.joinable()) t.join();
    }
    Renderer::workers.clear();

    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
    for (auto &[coord, chunk] : ChunkHelper::activeChunks) {
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

// void Renderer::renderAsyncChunks() {
//     while (!ChunkHelper::chunkBuildQueue.empty()) {
//         std::unique_ptr<Chunk> chunk;
//         while (ChunkHelper::chunkBuildQueue.try_pop(chunk)) {
//             chunk->model = buildChunkModel(*chunk);
//             chunk->loaded = true;
//             chunk->alpha = 0.0f;
//
//             replaceChunk(chunk->chunkCoords, std::move(chunk));
//         }
//     }
// }

// void Renderer::shutdown() {
//     // Stop workers first
//     ChunkHelper::workerRunning = false;
//     ChunkHelper::chunkRequestQueue.notifyAll();
//
//     for (auto &t: Renderer::workers) {
//         if (t.joinable()) t.join();
//     }
//     Renderer::workers.clear();
//
//     // Unload all active chunks safely
//     std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//     for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
//         if (!chunk) continue;
//
//         // Only unload if meshCount > 0
//         if (chunk->model.meshCount > 0 && IsModelValid(chunk->model)) {
//             UnloadModel(chunk->model);
//             chunk->model.meshCount = 0; // mark as unloaded
//         }
//     }
//     ChunkHelper::activeChunks.clear();
//
//     // Unload texture atlas safely
//     if (textureAtlas.id > 0 && IsTextureValid(textureAtlas)) {
//         UnloadTexture(textureAtlas);
//         textureAtlas.id = 0;
//     }
// }

// void Renderer::chunkWorkerThread() {
//     while (ChunkHelper::workerRunning) {
//         std::optional<ChunkCoord> coordOpt = ChunkHelper::chunkRequestQueue.wait_pop(ChunkHelper::workerRunning);
//         ChunkCoord coord;
//         if (!coordOpt.has_value()) continue;
//
//         coord = coordOpt.value();
//         auto chunk = std::make_unique<Chunk>();
//         chunk->chunkCoords = coord;
//
//         ChunkHelper::generateChunkTerrain(chunk);
//         ChunkHelper::populateTrees(chunk);
//
//         chunk->dirty = true;
//
//         ChunkHelper::chunkBuildQueue.push(std::move(chunk));
//
//         {
//             std::lock_guard<std::mutex> lock(ChunkHelper::chunkRequestSetMutex);
//             ChunkHelper::chunkRequestSet.erase(coord);
//         }
//     }
// }

void Renderer::initChunkWorkers(int threadCount) {
    ChunkHelper::workerRunning = true;
    for (int i = 0; i < threadCount; i++) {
        Renderer::workers.emplace_back([]() {
            while (ChunkHelper::workerRunning) {
                ChunkCoord coord;
                try { coord = ChunkHelper::chunkRequestQueue.wait_pop(ChunkHelper::workerRunning); } catch (...) {
                    continue;
                }
                if (!ChunkHelper::workerRunning) break;
                auto chunk = ChunkHelper::generateChunkAsync({(float) coord.x, 0.0f, (float) coord.z});
                ChunkHelper::chunkBuildQueue.push(std::move(chunk));
            }
        });
    }
}

// void Renderer::shutdownChunkWorkers() {
//     ChunkHelper::workerRunning = false;
//
//     ChunkHelper::chunkRequestQueue.notifyAll();
//
//     for (auto &t: workers)
//         if (t.joinable()) t.join();
//
//     workers.clear();
//
//     std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
//     for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
//         if (chunk && IsModelValid(chunk->model)) {
//             UnloadModel(chunk->model);
//         }
//     }
//
//     ChunkHelper::activeChunks.clear();
//
//     if (IsTextureValid(textureAtlas)) {
//         UnloadTexture(textureAtlas);
//     }
// }

void Renderer::processChunkBuildQueue(const Camera3D &camera) {
    constexpr int MAX_CHUNKS_PER_FRAME = 2;
    int processed = 0;

    while (processed < MAX_CHUNKS_PER_FRAME) {
        std::unique_ptr<Chunk> chunk;
        if (!ChunkHelper::chunkBuildQueue.try_pop(chunk)) break;
        if (!chunk) continue;

        ChunkCoord playerChunk = getPlayerChunkCoord(camera);
        int dx = abs(chunk->chunkCoords.x - playerChunk.x);
        int dz = abs(chunk->chunkCoords.z - playerChunk.z);
        if (dx > Settings::preLoadDistance || dz > Settings::preLoadDistance) {
            ChunkHelper::chunkBuildQueue.push(std::move(chunk));
            break;
        }

        // Build all three models
        buildChunkModel(*chunk);

        chunk->loaded = true;
        chunk->dirty = false;
        chunk->alpha = 0.0f;

        replaceChunk(chunk->chunkCoords, std::move(chunk));
        processed++;
    }
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
void Renderer::rebuildDirtyChunks() {
    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

    for (auto &[coord, chunk] : ChunkHelper::activeChunks) {
        if (!chunk || !chunk->dirty) continue;

        // Unload old models
        if (chunk->opaqueModel.meshCount > 0 && IsModelValid(chunk->opaqueModel)) {
            UnloadModel(chunk->opaqueModel);
            chunk->opaqueModel = {0};
        }
        if (chunk->translucentModel.meshCount > 0 && IsModelValid(chunk->translucentModel)) {
            UnloadModel(chunk->translucentModel);
            chunk->translucentModel = {0};
        }

        // Rebuild both models
        buildChunkModel(*chunk);

        chunk->dirty = false;
        chunk->loaded = true;
    }
}

Shader Renderer::waterShader = {0};
int Renderer::waterTimeLoc = 0;

void Renderer::initWaterShader() {
    const char* vsCode = R"(
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

    const char* fsCode = R"(
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