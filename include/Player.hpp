//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_PLAYER_HPP
#define REFACTOREDCLONE_PLAYER_HPP

#pragma once
#include <raylib.h>

class Player {
public:
    Player();
    void update();
    Camera3D& getCamera();
private:
    void move();
    Camera3D camera;
};


#endif //REFACTOREDCLONE_PLAYER_HPP