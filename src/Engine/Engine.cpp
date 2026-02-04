//
// Created by Tristan on 1/30/26.
//

#include "Engine.hpp"
#include <raylib.h>

#include "Engine/Rendering/Renderer.hpp"

#include <print>
#include <ranges>

#include "Biome/Biome.hpp"
#include "Menu/Menu.hpp"
#include "MultiThreading/MeshThreadPool.hpp"
#include "Settings.hpp"

Engine::Engine() {
    Engine::initWindowData();
    Engine::loadBlockTextures();
    MainMenuUI::init();
    const auto time = std::chrono::high_resolution_clock::now();
    const int temp = static_cast<int>(time.time_since_epoch().count());
    SetRandomSeed(temp);
    Renderer::initWaterShader();

    int threads = std::clamp((int)std::thread::hardware_concurrency() - 1, 2, 6);

    Settings::worldSeed = static_cast<int>(Settings::getSysTimeAsFloat());

    Renderer::initChunkWorkers(threads); // Chunk generation threads
    Renderer::initMeshThreadPool(2);     // Mesh building threads (2 is enough)

    this->player = std::make_unique<Player>();

    while (!WindowShouldClose() && Settings::gameStateFlag != GameStates::QUIT) {
        this->update();
        this->draw();
    }

    CloseWindow();
}

void Engine::update() {
    double frameStart = GetTime();
    double frameBudget = 1.0 / 60.0; // 16.6ms for 60fps

    auto hasTimeBudget = [&]() {
        return (GetTime() - frameStart) < frameBudget;
    };

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (Settings::gameStateFlag == GameStates::IN_GAME) {
            Settings::gameStateFlag = GameStates::MENU;
            EnableCursor();
        } else if (Settings::gameStateFlag == GameStates::MENU && Settings::previousGameState == GameStates::IN_GAME) {
            Settings::gameStateFlag = GameStates::IN_GAME;
            DisableCursor();
        }
    }

    if (Settings::gameStateFlag == GameStates::IN_GAME) {
        // Always do player update first (critical for responsiveness)
        this->player->update();

        // Chunk loading - always run to keep world populated
        if (hasTimeBudget()) {
            Renderer::checkActiveChunks(this->player->getCamera());
        }

        // Process build queue - can be deferred if over budget
        if (hasTimeBudget()) {
            Renderer::processChunkBuildQueue(this->player->getCamera());
        }

        // GPU uploads - important for visual updates
        if (hasTimeBudget()) {
            Renderer::uploadPendingMeshes();
        }

        // Water shader always updates (cheap)
        Renderer::updateWaterShader(static_cast<float>(GetTime()));

        // Dirty chunk rebuilds - can be deferred
        if (hasTimeBudget()) {
            Renderer::rebuildDirtyChunks();
        }

        // Renderer::unloadChunks(this->player->getCamera());
        coords = std::to_string(this->player->getCamera().position.x) + ", " +
                 std::to_string(this->player->getCamera().position.y) + ", " +
                 std::to_string(this->player->getCamera().position.z);
    }

    if (Settings::gameStateFlag == GameStates::QUIT) {
        WindowShouldClose();
        CloseWindow();
    }
}

void Engine::draw() const {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    if (Settings::gameStateFlag == GameStates::IN_GAME) {
        if (!this->player->isSpawned()) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {30, 30, 30, 255});

            const char* loadingText = "Loading world...";
            int textWidth = MeasureText(loadingText, 40);
            DrawText(loadingText, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 20,
                     40, WHITE);

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
            DrawText(progressText, (GetScreenWidth() - progressWidth) / 2,
                     GetScreenHeight() / 2 + 30, 20, LIGHTGRAY);
        } else {
            BeginMode3D(this->player->getCamera());
            Renderer::drawAllChunks(this->player->getCamera());
            EndMode3D();
            Renderer::drawCrosshair();
        }
    }

    DrawFPS(10, 10);
    if (this->player->isSpawned()) {
    }
    auto loc = this->player->getCamera().position;
    const char* temp = std::to_string(ChunkHelper::temperatureNoise.GetNoise(loc.x, loc.z)).c_str();

    DrawText(coords.c_str(), 10, 30, 16, WHITE);
    DrawText(BiomesHelper::getBiomeName(
                 ChunkHelper::getBiomeAt((int)this->player->getCamera().position.x,
                                         (int)this->player->getCamera().position.z))
                 .c_str(),
             10, 60, 16, WHITE);
    DrawText(std::to_string(Settings::worldSeed).c_str(), 10, 100, 16, WHITE);
    DrawText(temp, 10, 140, 16, WHITE);
    if (Settings::gameStateFlag == GameStates::MENU) {
        DrawTexture(menuBackgroundTexture, 0, 0, WHITE);
        MainMenuUI::draw();
    }
    EndDrawing();
}

void Engine::initWindowData() {
    InitWindow(Settings::screenWidth, Settings::screenHeight, "Fakecraft");
    SetTargetFPS(Settings::frameRate);
    SetExitKey(KEY_NULL);
}

void Engine::loadBlockTextures() {
    Renderer::textureAtlas = LoadTexture("../assets/textures/TextureAtlas.png");
    this->menuBackgroundTexture = LoadTexture("../assets/textures/menuBackground.png");
}

Engine::~Engine() {
    // header.seed = Settings::worldSeed;
    // if (header.version == nullptr) {
    //     header.version += 1;
    // }
    // SaveFileData("./Worlds/world.dat", &header, sizeof(header));

    MainMenuUI::unload();
    Renderer::shutdownMeshThreadPool(); // Shutdown mesh threads first
    Renderer::shutdown();               // Then chunk workers
}

