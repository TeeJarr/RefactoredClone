//
// Created by Tristan on 1/30/26.
//

#include "Player.hpp"
#include "Engine/Settings.hpp"
#include <raymath.h>
#include <raylib.h>
#include "Block/Blocks.hpp"
#include "Engine/Rendering/Renderer.hpp"
#include "World/Chunk/Chunk.hpp"
#include <cfloat>
#include <algorithm>

#include "../Collision/Collision.hpp"
#include "../Lighitng/LightingSystem.hpp"

Player::Player() {
    this->camera = {0};
    this->camera.position = (Vector3){0.0f, 120.0f, 10.0f}; // Camera position
    this->camera.target = (Vector3){0.0f, 0.0f, 0.0f}; // Camera looking at point
    this->camera.up = (Vector3){0.0f, 1.0f, 0.0f}; // Camera up vector (rotation towards target)
    this->camera.fovy = Settings::fov; // Camera field-of-view Y
    this->camera.projection = CAMERA_PERSPECTIVE; // Camera mode type
}

Camera3D &Player::getCamera() {
    return this->camera;
}

void Player::move() {
    UpdateCameraPro(&camera,
                    (Vector3){
                        (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) * 0.25f - // Move forward-backward
                        (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) * 0.25f,
                        (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) * 0.25f - // Move right-left
                        (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) * 0.25f,
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

    int x = (int) floor(rayPos.x);
    int y = (int) floor(rayPos.y);
    int z = (int) floor(rayPos.z);

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

        if (block != ID_AIR && block != ID_WATER && block != ID_BEDROCK) {
            // Set block to air
            ChunkHelper::setBlock(x, y, z, ID_AIR);

            // Mark this chunk dirty
            ChunkCoord coord = ChunkHelper::worldToChunkCoord(x, z);
            ChunkHelper::markChunkDirty(coord);

            auto &chunk = ChunkHelper::activeChunks.at(coord);
            LightingSystem::calculateChunkLighting(*chunk);
            LightingSystem::debugChunkLight(*chunk);


            // Check neighboring chunks if block is on edge
            int localX = ChunkHelper::WorldToLocal(x);
            int localZ = ChunkHelper::WorldToLocal(z);

            if (localX == 0)
                ChunkHelper::markChunkDirty({coord.x - 1, coord.z});
            else if (localX == CHUNK_SIZE_X - 1)
                ChunkHelper::markChunkDirty({coord.x + 1, coord.z});

            if (localZ == 0)
                ChunkHelper::markChunkDirty({coord.x, coord.z - 1});
            else if (localZ == CHUNK_SIZE_Z - 1)
                ChunkHelper::markChunkDirty({coord.x, coord.z + 1});

            return;
        }

        // Step to next voxel
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


// In Player.cpp
void Player::placeBlock() {
    if (!IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) return;

    constexpr float MAX_REACH = 5.0f;

    Vector3 rayPos = camera.position;
    Vector3 rayDir = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position)
    );

    int x = (int) floor(rayPos.x);
    int y = (int) floor(rayPos.y);
    int z = (int) floor(rayPos.z);

    int prevX = x, prevY = y, prevZ = z; // Track previous position

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
            // Place block at previous (empty) position
            if (prevY >= 0 && prevY < CHUNK_SIZE_Y) {
                ChunkHelper::setBlock(prevX, prevY, prevZ, ID_STONE); // Or selected block

                ChunkCoord coord = ChunkHelper::worldToChunkCoord(prevX, prevZ);
                ChunkHelper::markChunkDirty(coord);

                int localX = ChunkHelper::WorldToLocal(prevX);
                int localZ = ChunkHelper::WorldToLocal(prevZ);

                if (localX == 0)
                    ChunkHelper::markChunkDirty({coord.x - 1, coord.z});
                else if (localX == CHUNK_SIZE_X - 1)
                    ChunkHelper::markChunkDirty({coord.x + 1, coord.z});

                if (localZ == 0)
                    ChunkHelper::markChunkDirty({coord.x, coord.z - 1});
                else if (localZ == CHUNK_SIZE_Z - 1)
                    ChunkHelper::markChunkDirty({coord.x, coord.z + 1});
            }
            return;
        }

        // Save current position as previous
        prevX = x;
        prevY = y;
        prevZ = z;

        // Step to next voxel
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

// In Player.cpp

void Player::update() {
    if (!spawned) {
        trySpawn();
        return;
    }

    float deltaTime = GetFrameTime();

    // Mouse look
    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.002f;

    // Update camera angles
    static float yaw = 0.0f;
    static float pitch = 0.0f;

    yaw -= mouseDelta.x * sensitivity;
    pitch -= mouseDelta.y * sensitivity;

    // Clamp pitch
    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    // Calculate look direction
    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };

    // Apply gravity
    applyGravity(deltaTime);

    // Handle movement input
    handleMovement(deltaTime);

    // Update camera position (at eye level)
    camera.position = {
        position.x,
        position.y + EYE_HEIGHT,
        position.z
    };

    // Update camera target
    camera.target = Vector3Add(camera.position, forward);
// #endif
    this->breakBlock();
    this->placeBlock();
}

void Player::applyGravity(float deltaTime) {
    // Skip gravity when flying
    if (flying) {
        return;
    }

    if (!onGround) {
        velocity.y += GRAVITY * deltaTime;

        if (velocity.y < -50.0f) velocity.y = -50.0f;
    }

    // Apply vertical movement
    Vector3 newPosition = position;
    newPosition.y += velocity.y * deltaTime;

    // Check collision with small offset to avoid floating point issues
    AABB playerBox = Collision::getPlayerAABB(newPosition);

    // Shrink box slightly for ground check
    playerBox.min.x += 0.01f;
    playerBox.min.z += 0.01f;
    playerBox.max.x -= 0.01f;
    playerBox.max.z -= 0.01f;

    if (Collision::checkCollision(playerBox)) {
        if (velocity.y < 0) {
            // Falling - land on ground
            onGround = true;
            velocity.y = 0;

            // Snap to top of block
            newPosition.y = floor(newPosition.y) + 0.001f;

            // Adjust up until not colliding
            for (int i = 0; i < 10; i++) {
                playerBox = Collision::getPlayerAABB(newPosition);
                playerBox.min.x += 0.01f;
                playerBox.min.z += 0.01f;
                playerBox.max.x -= 0.01f;
                playerBox.max.z -= 0.01f;

                if (!Collision::checkCollision(playerBox)) break;
                newPosition.y += 0.1f;
            }
        } else {
            // Hit ceiling
            velocity.y = 0;
            newPosition = position;
        }
    } else {
        // Check if we're still on ground (small ray cast down)
        Vector3 groundCheck = newPosition;
        groundCheck.y -= 0.05f;

        AABB groundBox = Collision::getPlayerAABB(groundCheck);
        groundBox.min.x += 0.01f;
        groundBox.min.z += 0.01f;
        groundBox.max.x -= 0.01f;
        groundBox.max.z -= 0.01f;

        onGround = Collision::checkCollision(groundBox);
    }


    position = newPosition;
}

// Vector3 Player::moveWithCollision(Vector3 from, Vector3 to) {
//     Vector3 result = to;
//
//     // Use slightly smaller box for movement to avoid getting stuck
//     auto getShrunkAABB = [](const Vector3& pos) {
//         AABB box = Collision::getPlayerAABB(pos);
//         box.min.x += 0.02f;
//         box.min.z += 0.02f;
//         box.min.y += 0.01f;  // Small ground offset
//         box.max.x -= 0.02f;
//         box.max.z -= 0.02f;
//         return box;
//     };
//
//     // Try full movement first
//     AABB box = getShrunkAABB(result);
//     if (!Collision::checkCollision(box)) {
//         return result;
//     }
//
//     // Try X movement only
//     result = {to.x, from.y, from.z};
//     box = getShrunkAABB(result);
//     if (Collision::checkCollision(box)) {
//         result.x = from.x;
//     }
//
//     // Try Z movement only
//     result.z = to.z;
//     box = getShrunkAABB(result);
//     if (Collision::checkCollision(box)) {
//         result.z = from.z;
//     }
//
//     return result;
// }
void Player::handleMovement(float deltaTime) {
    // Double-tap space to toggle flight mode
    float currentTime = (float)GetTime();
    if (IsKeyPressed(KEY_SPACE)) {
        if (currentTime - lastSpacePressTime < DOUBLE_TAP_THRESHOLD) {
            flying = !flying;
            if (flying) {
                velocity.y = 0; // Stop falling when entering flight mode
                onGround = false;
            }
        }
        lastSpacePressTime = currentTime;
    }

    // Get movement input
    Vector3 moveDir = {0, 0, 0};

    // Calculate forward/right vectors (ignore pitch for movement)
    float yaw = atan2f(camera.target.x - camera.position.x,
                       camera.target.z - camera.position.z);

    Vector3 forward = {sinf(yaw), 0, cosf(yaw)};
    Vector3 right = {cosf(yaw), 0, -sinf(yaw)};

    if (IsKeyDown(KEY_W)) {
        moveDir.x += forward.x;
        moveDir.z += forward.z;
    }
    if (IsKeyDown(KEY_S)) {
        moveDir.x -= forward.x;
        moveDir.z -= forward.z;
    }
    if (IsKeyDown(KEY_D)) {
        moveDir.x -= right.x;
        moveDir.z -= right.z;
    }
    if (IsKeyDown(KEY_A)) {
        moveDir.x += right.x;
        moveDir.z += right.z;
    }

    // Normalize movement direction
    float length = sqrtf(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
    if (length > 0) {
        moveDir.x /= length;
        moveDir.z /= length;
    }

    // Apply speed (flying is faster)
    float speed = flying ? FLY_SPEED : MOVE_SPEED;
    if (IsKeyDown(KEY_LEFT_SHIFT) && !flying) {
        speed *= SPRINT_MULTIPLIER;
    }

    moveDir.x *= speed * deltaTime;
    moveDir.z *= speed * deltaTime;

    // Vertical movement
    if (flying) {
        // Flying: Space to go up, Shift to go down
        if (IsKeyDown(KEY_SPACE)) {
            position.y += FLY_VERTICAL_SPEED * deltaTime;
        }
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            position.y -= FLY_VERTICAL_SPEED * deltaTime;
        }
        // Clamp to world bounds
        position.y = std::clamp(position.y, 1.0f, 250.0f);
    } else {
        // Normal: Jump when on ground (single press, not the double-tap)
        // The jump is triggered only if it wasn't a double-tap toggle
        if (IsKeyDown(KEY_SPACE) && onGround &&
            (currentTime - lastSpacePressTime >= DOUBLE_TAP_THRESHOLD || !IsKeyPressed(KEY_SPACE))) {
            velocity.y = JUMP_VELOCITY;
            onGround = false;
        }
    }

    // Apply horizontal movement with collision (skip collision in flight for smoother movement)
    if (length > 0) {
        if (flying) {
            position.x += moveDir.x;
            position.z += moveDir.z;
        } else {
            position = moveWithCollision(position, {
                                             position.x + moveDir.x,
                                             position.y,
                                             position.z + moveDir.z
                                         });
        }
    }
}

Vector3 Player::moveWithCollision(Vector3 from, Vector3 to) {
    Vector3 result = to;

    // Try full movement first
    AABB box = Collision::getPlayerAABB(result);
    if (!Collision::checkCollision(box)) {
        return result;
    }

    // Try X movement only
    result = {to.x, from.y, from.z};
    box = Collision::getPlayerAABB(result);
    if (Collision::checkCollision(box)) {
        result.x = from.x; // Block X movement
    }

    // Try Z movement only
    result.z = to.z;
    box = Collision::getPlayerAABB(result);
    if (Collision::checkCollision(box)) {
        result.z = from.z; // Block Z movement
    }

    return result;
}
// In Player.cpp

void Player::trySpawn() {
    if (spawned) return;

    ChunkCoord spawnChunk = ChunkHelper::worldToChunkCoord(0, 0);

    // Wait for spawn chunk AND adjacent chunks
    std::vector<ChunkCoord> requiredChunks = {
        spawnChunk,
        {spawnChunk.x - 1, spawnChunk.z},
        {spawnChunk.x + 1, spawnChunk.z},
        {spawnChunk.x, spawnChunk.z - 1},
        {spawnChunk.x, spawnChunk.z + 1},
    };

    {
        std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);

        for (const auto& coord : requiredChunks) {
            auto it = ChunkHelper::activeChunks.find(coord);
            if (it == ChunkHelper::activeChunks.end() || !it->second || !it->second->loaded) {
                // Not all chunks ready
                return;
            }
        }
    }

    // Find a valid spawn position
    int spawnX = 8;  // Center of chunk
    int spawnZ = 8;
    int spawnY = -1;

    // Find the highest solid block
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
        int blockId = ChunkHelper::getBlock(spawnX, y, spawnZ);

        if (blockId != ID_AIR && blockId != ID_WATER) {
            spawnY = y + 1;  // Spawn on top of this block
            break;
        }
    }

    if (spawnY < 0) {
        // No valid ground found, try again later
        return;
    }

    // Check there's space for the player (2 blocks of air above)
    int blockAbove1 = ChunkHelper::getBlock(spawnX, spawnY, spawnZ);
    int blockAbove2 = ChunkHelper::getBlock(spawnX, spawnY + 1, spawnZ);

    if (blockAbove1 != ID_AIR || blockAbove2 != ID_AIR) {
        for (int dx = -5; dx <= 5; dx++) {
            for (int dz = -5; dz <= 5; dz++) {
                int testX = spawnX + dx;
                int testZ = spawnZ + dz;

                for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                    int blockId = ChunkHelper::getBlock(testX, y, testZ);

                    if (blockId != ID_AIR && blockId != ID_WATER) {
                        int above1 = ChunkHelper::getBlock(testX, y + 1, testZ);
                        int above2 = ChunkHelper::getBlock(testX, y + 2, testZ);

                        if (above1 == ID_AIR && above2 == ID_AIR) {
                            spawnX = testX;
                            spawnZ = testZ;
                            spawnY = y + 1;
                            goto foundSpawn;
                        }
                        break;
                    }
                }
            }
        }

        return;
    }

foundSpawn:
    // Set player position
    position = {(float)spawnX + 0.5f, (float)spawnY, (float)spawnZ + 0.5f};
    velocity = {0, 0, 0};
    onGround = true;

    // Update camera
    camera.position = {position.x, position.y + EYE_HEIGHT, position.z};
    camera.target = {position.x, position.y + EYE_HEIGHT, position.z + 1.0f};

    spawned = true;
    waitingForChunks = false;

    // Disable cursor for gameplay
    DisableCursor();

    printf("Player spawned at (%.1f, %.1f, %.1f)\n", position.x, position.y, position.z);
}