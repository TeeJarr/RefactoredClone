//
// Created by Tristan on 1/30/26.
//

#include "../include/Player.hpp"
#include "Settings.hpp"
#include <raymath.h>
#include <raylib.h>
#include "Blocks.hpp"
#include "Renderer.hpp"
#include "Chunk.hpp"
#include <cfloat>

Player::Player() {
    this->camera = {0};
    this->camera.position = (Vector3){0.0f, 65.0f, 10.0f}; // Camera position
    this->camera.target = (Vector3){0.0f, 0.0f, 0.0f}; // Camera looking at point
    this->camera.up = (Vector3){0.0f, 1.0f, 0.0f}; // Camera up vector (rotation towards target)
    this->camera.fovy = Settings::fov; // Camera field-of-view Y
    this->camera.projection = CAMERA_PERSPECTIVE; // Camera mode type
}

void Player::update() {
    this->move();
    this->breakBlock();
}

Camera3D &Player::getCamera() {
    return this->camera;
}

void Player::move() {
    UpdateCameraPro(&camera,
                    (Vector3){
                        (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) * 0.1f - // Move forward-backward
                        (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) * 0.1f,
                        (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) * 0.1f - // Move right-left
                        (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) * 0.1f,
                        (IsKeyDown(KEY_SPACE) * 0.1f - IsKeyDown(KEY_LEFT_SHIFT) * 0.2f) // Move up-down
                    },
                    (Vector3){
                        GetMouseDelta().x * 0.05f, // Rotation: yaw
                        GetMouseDelta().y * 0.05f, // Rotation: pitch
                        0.0f // Rotation: roll
                    },
                    GetMouseWheelMove() * 2.0f); // Move to target (zoom)
}

void Player::breakBlock() {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;

    constexpr float MAX_REACH = 5.0f;

    Vector3 rayPos = camera.position;
    Vector3 rayDir = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position)
    );

    int x = (int)floor(rayPos.x);
    int y = (int)floor(rayPos.y);
    int z = (int)floor(rayPos.z);

    int stepX = (rayDir.x > 0) ? 1 : -1;
    int stepY = (rayDir.y > 0) ? 1 : -1;
    int stepZ = (rayDir.z > 0) ? 1 : -1;

    auto intBound = [](float s, float ds) {
        if (ds > 0) return (ceilf(s) - s) / ds;
        if (ds < 0) return (s - floorf(s)) / -ds;
        return FLT_MAX;
    };

    auto safeInv = [](float v) {
        return (fabs(v) < 1e-6f) ? 1e30f : fabs(1.0f / v);
    };

    float tMaxX = intBound(rayPos.x, rayDir.x);
    float tMaxY = intBound(rayPos.y, rayDir.y);
    float tMaxZ = intBound(rayPos.z, rayDir.z);

    float tDeltaX = safeInv(rayDir.x);
    float tDeltaY = safeInv(rayDir.y);
    float tDeltaZ = safeInv(rayDir.z);

    float dist = 0.0f;

    while (dist <= MAX_REACH) {
        int block = ChunkHelper::getBlock(x, y, z);

        if (block != ID_AIR && block != ID_WATER) {
            ChunkHelper::setBlock(x, y, z, ID_AIR);

            // 🔥 THIS IS THE KEY PART 🔥
            ChunkCoord coord = ChunkHelper::worldToChunkCoord(x, z);
            ChunkHelper::markChunkDirty(coord);

            if (x % CHUNK_SIZE_X == 0)
                ChunkHelper::markChunkDirty({coord.x - 1, coord.z});
            if (x % CHUNK_SIZE_X == CHUNK_SIZE_X - 1)
                ChunkHelper::markChunkDirty({coord.x + 1, coord.z});
            if (z % CHUNK_SIZE_Z == 0)
                ChunkHelper::markChunkDirty({coord.x, coord.z - 1});
            if (z % CHUNK_SIZE_Z == CHUNK_SIZE_Z - 1)
                ChunkHelper::markChunkDirty({coord.x, coord.z + 1});

            return;
        }



        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                dist = tMaxX;
                tMaxX += tDeltaX;
            } else {
                z += stepZ;
                dist = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                dist = tMaxY;
                tMaxY += tDeltaY;
            } else {
                z += stepZ;
                dist = tMaxZ;
                tMaxZ += tDeltaZ;
            }
        }
    }
}


std::unique_ptr<Chunk> *Player::getCurrentPlayerChunk() {
    ChunkCoord coord = Renderer::getPlayerChunkCoord(this->getCamera());

    auto it = ChunkHelper::activeChunks.find(coord);
    if (it != ChunkHelper::activeChunks.end()) {
        return &it->second; // return pointer to unique_ptr
    }
    return nullptr;
}
