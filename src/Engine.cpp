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
    int threads = std::clamp(
    (int)std::thread::hardware_concurrency() - 1,
    2,
    4
    );
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
    coords = "x: " + std::to_string(this->player->getCamera().position.x) + "\ny: " + std::to_string(this->player->getCamera().position.y) + ",\nz: " + std::to_string(this->player->getCamera().position.z);
    this->player->update();
    // Renderer::unloadChunks(this->player->getCamera());
}

void Engine::draw() const {
    BeginDrawing();
    ClearBackground(SKYBLUE);
        BeginMode3D(this->player->getCamera());
        for (auto& [coord, chunk] : ChunkHelper::activeChunks) {
            Renderer::drawChunk(chunk, this->player->getCamera());
        }
    EndMode3D();
    DrawFPS(10,10);
    DrawText(coords.c_str(),10 ,30, 16, WHITE);
    Renderer::drawCrosshair();
    EndDrawing();
}

// TODO: replace with settings values
void Engine::initWindowData() {
    InitWindow(1280, 720, "Fakecraft");
    // SetTargetFPS(60);
    // SetWindowState(FLAG_FULLSCREEN_MODE);
    DisableCursor();
    SetRandomSeed(Settings::worldSeed);
}

void Engine::initModels() {
    Renderer::createCubeModels();
}

void Engine::loadBlockTextures() {
    Renderer::textureAtlas = LoadTexture("../assets/textures/TextureAtlas.png");
}
