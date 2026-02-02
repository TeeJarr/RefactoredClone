//
// Created by Tristan on 2/1/26.
//

#include "../include/Menu.hpp"
#include <raygui.h>
#include <string>

#include "Settings.hpp"

int HashSeed(const std::string& s){
    int hash = 0;
    for (unsigned char c : s){
        hash = hash * 31 + c; // Java-style
    }
    return hash;
}

void MainMenu()
{
    static bool showSeedDialog = false;
    static char seedText[32] = { 0 };
    static bool secretView = false;

    // --- Load World button ---
    if (GuiButton(
        { GetScreenWidth() / 2.0f - 50, GetScreenHeight() / 2.0f, 100, 40 },
        "Load World"
    ))
    {
        showSeedDialog = true;
    }

    // --- Seed dialog ---
    if (showSeedDialog)
    {
        int result = GuiTextInputBox(
            {
                GetScreenWidth() / 2.0f - 150,
                GetScreenHeight() / 2.0f - 75,
                300,
                150
            },
            "World Seed",
            "Enter a seed",
            "OK;Cancel",
            seedText,
            32,
            &secretView
        );

        if (result == 1) // OK
        {
            std::string seed(seedText);
            showSeedDialog = false;

            Settings::worldSeed = HashSeed(seed);
            DisableCursor();
            Settings::gameStateFlag = IN_GAME;
        }
        else if (result == 2) // Cancel
        {
            showSeedDialog = false;
        }
    }
}

#include <cstdint>

