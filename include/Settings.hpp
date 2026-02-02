//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_SETTINGS_HPP
#define REFACTOREDCLONE_SETTINGS_HPP

enum GameStates {
    MENU,
    IN_GAME
};

class Settings {
public:
    static int screenWidth;
    static int screenHeight;
    static int frameRate;

    static int renderDistance; // Rendering Chunks
    static float fov;
    static int unloadDistance;
    static int preLoadDistance;

    static int worldSeed;

    static int gameStateFlag;

    static float getSysTimeAsFloat();
};


#endif //REFACTOREDCLONE_SETTINGS_HPP
