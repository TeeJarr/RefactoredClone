//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_ENGINE_HPP
#define REFACTOREDCLONE_ENGINE_HPP
#pragma once
#include <memory>

#include "Player.hpp"


class Engine {
public:
    Engine();

private:
    void draw() const;
    void update();

    static void initWindowData();
    void initModels();
    static void loadBlockTextures();

private:
    std::unique_ptr<Player> player;
    Model cubeModel{};
    Mesh mesh{};
};


#endif //REFACTOREDCLONE_ENGINE_HPP