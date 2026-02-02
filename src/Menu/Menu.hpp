//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_MENU_HPP
#define REFACTOREDCLONE_MENU_HPP

#pragma once
#include "raylib.h"
#include <string>

enum MenuState {
    MENU_MAIN,
    MENU_WORLD_SELECT,
    MENU_SEED_INPUT,
    MENU_OPTIONS
};

class MainMenuUI {
public:
    static void init();
    static void draw();
    static void unload();

private:
    static void drawBackground();
    static void drawMainMenu();
    static void drawWorldSelect();
    static void drawSeedInput();
    static void drawOptions();

    static bool drawButton(Rectangle bounds, const char* text, bool enabled = true);
    static bool drawTextInput(Rectangle bounds, char* text, int maxLength, bool active);
    static void drawTitle(const char* text, int y);
    static void drawSplashText(const char* text, int x, int y, float rotation);

    static Texture2D dirtTexture;
    static Texture2D logoTexture;
    static bool texturesLoaded;

    static MenuState currentState;
    static char seedText[32];
    static bool seedInputActive;
    static float titleBob;
    static float splashScale;
    static float splashScaleDir;

    static int hashSeed(const std::string& seed);
};
#endif //REFACTOREDCLONE_MENU_HPP