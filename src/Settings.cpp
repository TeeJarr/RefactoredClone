//
// Created by Tristan on 1/30/26.
//
#include "Settings.hpp"

int Settings::screenWidth = 1280;
int Settings::screenHeight = 720;
int Settings::frameRate = 60;

int Settings::renderDistance = 12; // Rendering Chunks
int Settings::preLoadDistance = Settings::renderDistance + 2;
int Settings::unloadDistance = Settings::renderDistance + 4;
float Settings::fov = 45.0f;

int Settings::worldSeed = 0;
