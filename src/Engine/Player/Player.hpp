//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_PLAYER_HPP
#define REFACTOREDCLONE_PLAYER_HPP

#pragma once
#include <memory>
#include <raylib.h>

#include "World/Chunk/Chunk.hpp"

class Player {
public:
    Player();
    void update();
    Camera3D& getCamera();

    void handleMovement(float deltaTime);
    void applyGravity(float deltaTime);
    void handleCollision(Vector3& newPosition);
    bool isOnGround();
    Vector3 moveWithCollision(Vector3 from, Vector3 to);

    void trySpawn();
    bool isSpawned() const { return spawned; }
private:
    void move();
    void breakBlock();
    Vector3 velocity = {0, 0, 0};
    bool onGround = false;

    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float EYE_HEIGHT = 1.62f;  // Eye level from feet
    const float GRAVITY = -28.0f;
    const float JUMP_VELOCITY = 9.0f;
    const float MOVE_SPEED = 5.0f;
    const float SPRINT_MULTIPLIER = 1.5f;

    Vector3 position{};  // Position at player's feet

    bool spawned = true;
    bool waitingForChunks = true;

    void placeBlock();
    int lastHitX{}, lastHitY{}, lastHitZ{};  // Track the block before the one we hit

    std::unique_ptr<Chunk>* getCurrentPlayerChunk();

    Camera3D camera{};
};


#endif //REFACTOREDCLONE_PLAYER_HPP