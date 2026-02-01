//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_PLAYER_HPP
#define REFACTOREDCLONE_PLAYER_HPP

#pragma once
#include <memory>
#include <raylib.h>

#include "Chunk.hpp"

class Player {
public:
    Player();
    void update();
    Camera3D& getCamera();
private:
    void move();
    void breakBlock();

    std::unique_ptr<Chunk>* getCurrentPlayerChunk();

    Camera3D camera;
};


#endif //REFACTOREDCLONE_PLAYER_HPP