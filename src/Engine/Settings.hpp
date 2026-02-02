//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_SETTINGS_HPP
#define REFACTOREDCLONE_SETTINGS_HPP
// Settings.hpp
#pragma once
#include <raylib.h>

enum GameStates {
    MENU,
    IN_GAME,
    PAUSED,
    QUIT
};

namespace Settings {
    inline int screenWidth = 1280;
    inline int screenHeight = 720;
    inline int frameRate = 12;
    inline int preLoadDistance = 8;

    inline int renderDistance = 12;
    inline int unloadDistance = renderDistance + 1;
    inline float fov = 70.0f;
    inline int worldSeed = 0;
    inline GameStates gameStateFlag = MENU;
    inline GameStates previousGameState = MENU;

    inline float getSysTimeAsFloat() {
        return (float)GetTime();
    }
}

#endif //REFACTOREDCLONE_SETTINGS_HPP
