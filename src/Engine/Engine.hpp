//
// Created by Tristan on 1/30/26.
//

#ifndef REFACTOREDCLONE_ENGINE_HPP
#define REFACTOREDCLONE_ENGINE_HPP
#pragma once
#include <memory>
#include "Chunk/Chunk.hpp"

#include "Player/Player.hpp"


class Engine {
public:
    Engine();

private:
    void draw() const;
    void update();

     void initWindowData();
     void loadBlockTextures();

private:
    std::unique_ptr<Player> player;
    std::string coords;
    Texture2D menuBackgroundTexture;

    ~Engine();
};


#endif //REFACTOREDCLONE_ENGINE_HPP