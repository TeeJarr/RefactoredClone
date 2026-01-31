//
// Created by Tristan on 1/30/26.
//

#include "Engine.hpp"
#include <raylib.h>

// #include <rlgl.h>
#include "Renderer.hpp"

#include <filesystem>
#include <print>

Engine::Engine() {
    Engine::initWindowData();
    Engine::loadBlockTextures();
    this->initModels();
    this->player = std::make_unique<Player>();

    while (!WindowShouldClose()) {
        this->update();
        this->draw();
    }

    CloseWindow();
}

void Engine::update() {
    this->player->update();
}

void Engine::draw() const {
    BeginDrawing();
    ClearBackground(BLACK);
        BeginMode3D(this->player->getCamera());
            DrawModel(cubeModel, {0,0,0}, 1.0f, WHITE);
        EndMode3D();
    DrawFPS(10,10);
    EndDrawing();
}

// TODO: replace with settings values
void Engine::initWindowData() {
    InitWindow(1280, 720, "Fakecraft");
    SetTargetFPS(60);
    DisableCursor();
}

void Engine::initModels() {
    cubeModel = Renderer::createCubeModel(cubeModel,mesh);
}

void Engine::loadBlockTextures() {
    Renderer::textureAtlas = LoadTexture("../assets/textures/TextureAtlas.png");
}
