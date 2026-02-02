//
// Created by Tristan on 2/1/26.
//

#ifndef REFACTOREDCLONE_MENU_HPP
#define REFACTOREDCLONE_MENU_HPP

#pragma once
#include <raylib.h>
#include <vector>

#include "raygui.h"

struct Menu {
    std::vector<GuiControl*> controls;
};

void MainMenu();

#endif //REFACTOREDCLONE_MENU_HPP