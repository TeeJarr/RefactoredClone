//
// Created by Tristan on 1/30/26.
//

#include "../include/Renderer.hpp"

#include <ostream>
#include <ranges>
#include <raylib.h>
#include <raymath.h>

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


void Renderer::drawChunk(const std::unique_ptr<Chunk> &chunk, const Camera3D &camera) {
    if (!isBoxOnScreen(chunk->boundingBox, camera)) return;
    if (!chunk->loaded) return;


    chunk->alpha += GetFrameTime() * 2.0f; // fade-in over ~0.5 seconds
    if (chunk->alpha > 1.0f) chunk->alpha = 1.0f;

    Vector3 worldPos = {
        (float) (chunk->chunkCoords.x * CHUNK_SIZE_X),
        0.0f,
        (float) (chunk->chunkCoords.z * CHUNK_SIZE_Z)
    };

    Color tint = WHITE;
    tint.a = (unsigned char) (chunk->alpha * 255);

    // std::println("Drawing chunk: {}, {}", chunk->chunkCoords.x, chunk->chunkCoords.z);
    DrawModel(chunk->model, worldPos, 1.0f, tint);
}

UVRect Renderer::GetTileUV(int tileIndex, int atlasWidth, int atlasHeight, int tileSize) {
    float u0 = 0.0f;
    float u1 = 1.0f; // full width
    float v0 = (float) (tileIndex * tileSize) / (float) atlasHeight;
    float v1 = (float) ((tileIndex + 1) * tileSize) / (float) atlasHeight;
    return {u0, v0, u1, v1};
}


void Renderer::createCubeModels() {
    for (auto &[id, model]: blockModels) {
        UnloadModel(model);
    }
    blockModels.clear();

    for (const auto &[id, def]: blockTextureDefs) {
        Mesh mesh = {0};

        float vertices[] = {
            // Front
            0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
            // Back
            1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1,
            // Left
            0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1,
            // Right
            1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0,
            // Top
            0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1,
            // Bottom
            0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0
        };

        unsigned short indices[] = {
            0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
            8, 10, 9, 8, 11, 10, 12, 13, 14, 12, 14, 15,
            16, 18, 17, 16, 19, 18, 20, 22, 21, 20, 23, 22
        };

        float normals[] = {
            0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1,
            0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
            -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0,
            1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
            0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0
        };

        float texcoords[24 * 2];
        int t = 0;
        for (int f = 0; f < 6; f++) {
            UVRect uv = GetAtlasUV(def.faceTile[f],
                                   textureAtlas.width,
                                   textureAtlas.height,
                                   TILE_WIDTH,
                                   TILE_HEIGHT);
            bool flipV = (f >= 0 && f <= 3); // sides

            for (int v = 0; v < 4; v++) {
                float u = uv.u0 + (uv.u1 - uv.u0) * FACE_UVS[v].x;
                float tv = uv.v0 + (uv.v1 - uv.v0) * (flipV ? 1.0f - FACE_UVS[v].y : FACE_UVS[v].y);
                texcoords[t++] = u;
                texcoords[t++] = tv;
            }
        }

        unsigned char colors[24 * 4];
        int c = 0;
        for (int f = 0; f < 6; f++) {
            float light = FACE_LIGHT[f];
            for (int v = 0; v < 4; v++) {
                if (def.tintTop && f == 4) {
                    colors[c++] = (unsigned char) (light * 105.0f);
                    colors[c++] = (unsigned char) (light * 175.0f);
                    colors[c++] = (unsigned char) (light * 59.0f);
                } else {
                    unsigned char l = (unsigned char) (light * 255.0f);
                    colors[c++] = l;
                    colors[c++] = l;
                    colors[c++] = l;
                }
                colors[c++] = 255;
            }
        }

        // Allocate mesh
        mesh.vertexCount = 24;
        mesh.triangleCount = 12;
        mesh.vertices = vertices;
        mesh.normals = normals;
        mesh.texcoords = texcoords;
        mesh.colors = colors;
        mesh.indices = indices;

        UploadMesh(&mesh, false);

        Model model = LoadModelFromMesh(mesh);
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textureAtlas;

        blockModels[id] = model; // store prebuilt model
    }
}

void Renderer::createCubeModel() {
    // blockModels[BlockIds::ID_GRASS] = LoadModelFromMesh(createCubeMeshWithAtlas(BlockIds::ID_GRASS));
    // blockModels[BlockIds::ID_DIRT]  = LoadModelFromMesh(createCubeMeshWithAtlas(BlockIds::ID_DIRT));
    // blockModels[BlockIds::ID_STONE] = LoadModelFromMesh(createCubeMeshWithAtlas(BlockIds::ID_STONE));
    //
    // for (const auto& model: blockModels | std::views::values) {
    //     model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Renderer::textureAtlas;
    // }
}

int Renderer::GetGrassTileForFace(int face) {
    switch (face) {
        case FACE_TOP: return 0; // grass top
        case FACE_BOTTOM: return 2; // dirt
        default: return 1; // grass side
    }
}

bool Renderer::isFaceExposed(const Chunk &chunk, int x, int y, int z, int face) {
    int nx = x + dx[face];
    int ny = y + dy[face];
    int nz = z + dz[face];

    if (nx < 0 || nx >= CHUNK_SIZE_X ||
        ny < 0 || ny >= CHUNK_SIZE_Y ||
        nz < 0 || nz >= CHUNK_SIZE_Z)
        return true;

    // Neighbor is air → exposed
    return chunk.blockPosition[nx][ny][nz] == BlockIds::ID_AIR;
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

    for (const auto &[coord, chunk]: ChunkHelper::activeChunks) {
        int dx = coord.x - playerChunk.x;
        int dz = coord.z - playerChunk.z;

        if (abs(dx) > Settings::unloadDistance ||
            abs(dz) > Settings::unloadDistance) {
            // 🔒 Only unload fully built chunks
            if (chunk && chunk->loaded) {
                toRemove.push_back(coord);
            }
        }
    }

    for (const auto &coord: toRemove) {
        auto it = ChunkHelper::activeChunks.find(coord);
        if (it == ChunkHelper::activeChunks.end()) continue;

        // MUST be main thread
        if (IsModelValid(it->second->model)) {
            assert(it->second->model.meshCount > 0);
            UnloadModel(it->second->model);
            it->second->loaded = false;
            ChunkHelper::activeChunks.erase(it);
        };
    }
}

int Renderer::worldToChunk(float v, int chunkSize) {
    int i = (int) floor(v);
    return (i >= 0) ? (i / chunkSize) : ((i - chunkSize + 1) / chunkSize);
}

void Renderer::renderAsyncChunks() {
    while (!ChunkHelper::chunkBuildQueue.empty()) {
        std::unique_ptr<Chunk> chunk;
        while (ChunkHelper::chunkBuildQueue.try_pop(chunk)) {
            chunk->model = buildChunkModel(*chunk);
            chunk->loaded = true;
            chunk->alpha = 0.0f;

            replaceChunk(chunk->chunkCoords, std::move(chunk));
        }
    }
}

// Renderer static init
void Renderer::init() {
    ChunkHelper::workerRunning = true;
}

void Renderer::shutdown() {
    // Stop workers first
    ChunkHelper::workerRunning = false;
    ChunkHelper::chunkRequestQueue.notifyAll();

    for (auto &t: Renderer::workers) {
        if (t.joinable()) t.join();
    }
    Renderer::workers.clear();

    // Unload all active chunks safely
    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
    for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
        if (!chunk) continue;

        // Only unload if meshCount > 0
        if (chunk->model.meshCount > 0 && IsModelValid(chunk->model)) {
            UnloadModel(chunk->model);
            chunk->model.meshCount = 0; // mark as unloaded
        }
    }
    ChunkHelper::activeChunks.clear();

    // Unload texture atlas safely
    if (textureAtlas.id > 0 && IsTextureValid(textureAtlas)) {
        UnloadTexture(textureAtlas);
        textureAtlas.id = 0;
    }
}

void Renderer::chunkWorkerThread() {
    while (ChunkHelper::workerRunning) {
        std::optional<ChunkCoord> coordOpt = ChunkHelper::chunkRequestQueue.wait_pop(ChunkHelper::workerRunning);
        ChunkCoord coord;
        if (!coordOpt.has_value()) continue;

        coord = coordOpt.value();
        auto chunk = std::make_unique<Chunk>();
        chunk->chunkCoords = coord;

        ChunkHelper::generateChunkTerrain(chunk);
        ChunkHelper::populateTrees(chunk);

        chunk->dirty = true;

        ChunkHelper::chunkBuildQueue.push(std::move(chunk));

        {
            std::lock_guard<std::mutex> lock(ChunkHelper::chunkRequestSetMutex);
            ChunkHelper::chunkRequestSet.erase(coord);
        }
    }
}

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

void Renderer::shutdownChunkWorkers() {
    ChunkHelper::workerRunning = false;

    ChunkHelper::chunkRequestQueue.notifyAll();

    for (auto &t: workers)
        if (t.joinable()) t.join();

    workers.clear();

    std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
    for (auto &[coord, chunk]: ChunkHelper::activeChunks) {
        if (chunk && IsModelValid(chunk->model)) {
            UnloadModel(chunk->model);
        }
    }

    ChunkHelper::activeChunks.clear();

    if (IsTextureValid(textureAtlas)) {
        UnloadTexture(textureAtlas);
    }
}

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

        chunk->model = buildChunkModel(*chunk);
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
Model Renderer::buildChunkModel(const Chunk &chunk) {
    ChunkMeshBuffers buf;

    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                BlockIds id = static_cast<BlockIds>(chunk.blockPosition[x][y][z]);
                if (id == ID_AIR) continue;

                auto def = blockTextureDefs.at(id);

                for (int f = 0; f < 6; f++) {
                    if (!Renderer::isFaceExposed(chunk, x, y, z, f)) continue;

                    // int tile = def.faceTile[f];   // Correct tile per face
                    bool tintTop = def.tintTop;

                    AddFace(buf, (Vector3){(float) x, (float) y, (float) z}, f, def, FACE_LIGHT[f], tintTop);
                }
            }

    // Upload mesh
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
        if (it->second->model.meshCount > 0) {
            UnloadModel(it->second->model);
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
