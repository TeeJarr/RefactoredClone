//
// Created by Tristan on 1/30/26.
//

#include "Engine.hpp"
#include <raylib.h>

// #include <rlgl.h>
#include "Renderer.hpp"
#include "Blocks.hpp"

#include <filesystem>
#include <print>

#include "Settings.hpp"

Engine::Engine() {
    Engine::initWindowData();
    Engine::loadBlockTextures();
    ChunkHelper::initNoiseRenderer();
    Renderer::initWaterShader();
    int threads = std::clamp(
    (int)std::thread::hardware_concurrency() - 1,
    2,
    6
    );
    std::println("{}", Settings::getSysTimeAsFloat());
    Settings::worldSeed = static_cast<int>(Settings::getSysTimeAsFloat());
    Renderer::initChunkWorkers(threads);
    this->player = std::make_unique<Player>();

    while (!WindowShouldClose()) {
        this->update();
        this->draw();
    }
    Renderer::shutdown();
    CloseWindow();
}

void Engine::update() {
    Renderer::checkActiveChunks(this->player->getCamera());
    Renderer::processChunkBuildQueue(this->player->getCamera());
    Renderer::updateWaterShader(static_cast<float>(GetTime()));
    coords = "x: " + std::to_string(this->player->getCamera().position.x) + "\ny: " + std::to_string(this->player->getCamera().position.y) + ",\nz: " + std::to_string(this->player->getCamera().position.z);
    this->player->update();
    // Renderer::unloadChunks(this->player->getCamera());
    Renderer::rebuildDirtyChunks();
}

void Engine::draw() const {
    BeginDrawing();
    ClearBackground(SKYBLUE);
        BeginMode3D(this->player->getCamera());
            Renderer::drawAllChunks(this->player->getCamera());
        EndMode3D();
    DrawFPS(10,10);
    DrawText(coords.c_str(),10 ,30, 16, BLACK);
    Renderer::drawCrosshair();
    EndDrawing();
}

void Engine::initWindowData() {
    InitWindow(Settings::screenWidth, Settings::screenHeight, "Fakecraft");
    SetTargetFPS(Settings::frameRate);
    DisableCursor();
}

void Engine::loadBlockTextures() {
    Renderer::textureAtlas = LoadTexture("../assets/textures/TextureAtlas.png");
}
