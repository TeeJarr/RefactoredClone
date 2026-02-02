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

#include "raygui.h"
#include "Settings.hpp"
#include "Menu.hpp"
#include "MeshThreadPool.hpp"

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

    Renderer::initChunkWorkers(threads);      // Chunk generation threads
    Renderer::initMeshThreadPool(2);          // Mesh building threads (2 is enough)

    this->player = std::make_unique<Player>();

    while (!WindowShouldClose()) {
        this->update();
        this->draw();
    }

    Renderer::shutdownMeshThreadPool();  // Shutdown mesh threads first
    Renderer::shutdown();                 // Then chunk workers
    CloseWindow();
}

void Engine::update() {
    double frameStart = GetTime();
    double framebudget = 1.0 / 60.0;  // 16.6ms for 60fps

    if (Settings::gameStateFlag == GameStates::IN_GAME) {
        Renderer::checkActiveChunks(this->player->getCamera());

        // Only process chunks if we have time
        // if (GetTime() - frameStart < framebudget * 0.5) {
            Renderer::processChunkBuildQueue(this->player->getCamera());
        // }

        Renderer::uploadPendingMeshes();
        this->player->update();

        // Only rebuild if we have time
        // if (GetTime() - frameStart < framebudget * 0.7) {
            Renderer::rebuildDirtyChunks();
        // }

        Renderer::updateWaterShader(static_cast<float>(GetTime()));
    }
}
// In Engine.cpp

void Engine::draw() const {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    if (Settings::gameStateFlag == GameStates::IN_GAME) {
        if (!this->player->isSpawned()) {
            // Show loading screen
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {30, 30, 30, 255});

            const char* loadingText = "Loading world...";
            int textWidth = MeasureText(loadingText, 40);
            DrawText(loadingText,
                     (GetScreenWidth() - textWidth) / 2,
                     GetScreenHeight() / 2 - 20,
                     40, WHITE);

            // Show chunk loading progress
            int chunksLoaded = 0;
            {
                std::lock_guard<std::mutex> lock(ChunkHelper::activeChunksMutex);
                for (auto& [coord, chunk] : ChunkHelper::activeChunks) {
                    if (chunk && chunk->loaded) chunksLoaded++;
                }
            }

            char progressText[64];
            snprintf(progressText, sizeof(progressText), "Chunks loaded: %d", chunksLoaded);
            int progressWidth = MeasureText(progressText, 20);
            DrawText(progressText,
                     (GetScreenWidth() - progressWidth) / 2,
                     GetScreenHeight() / 2 + 30,
                     20, LIGHTGRAY);
        } else {
            // Normal game rendering
            BeginMode3D(this->player->getCamera());
            Renderer::drawAllChunks(this->player->getCamera());
            EndMode3D();

            Renderer::drawCrosshair();
        }
    }

    DrawFPS(10, 10);

    if (this->player->isSpawned()) {
        DrawText(coords.c_str(), 10, 30, 16, BLACK);
    }

    if (Settings::gameStateFlag == GameStates::MENU) {
        MainMenu();
    }

    EndDrawing();
}

void Engine::initWindowData() {
    InitWindow(Settings::screenWidth, Settings::screenHeight, "Fakecraft");
    SetTargetFPS(Settings::frameRate);
    // DisableCursor();
}

void Engine::loadBlockTextures() {
    Renderer::textureAtlas = LoadTexture("../assets/textures/TextureAtlas.png");
}
