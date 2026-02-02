// Menu.cpp
#include "Menu.hpp"
#include "Settings.hpp"
#include "Chunk/Chunk.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

// Static member initialization
Texture2D MainMenuUI::dirtTexture = {0};
Texture2D MainMenuUI::logoTexture = {0};
bool MainMenuUI::texturesLoaded = false;
MenuState MainMenuUI::currentState = MENU_MAIN;
char MainMenuUI::seedText[32] = "";
bool MainMenuUI::seedInputActive = true;
float MainMenuUI::titleBob = 0.0f;
float MainMenuUI::splashScale = 1.0f;
float MainMenuUI::splashScaleDir = 1.0f;

// Colors
static const Color BUTTON_NORMAL = {80, 80, 80, 255};
static const Color BUTTON_HOVER = {110, 110, 110, 255};
static const Color BUTTON_DISABLED = {50, 50, 50, 255};
static const Color BUTTON_BORDER_LIGHT = {130, 130, 130, 255};
static const Color BUTTON_BORDER_DARK = {30, 30, 30, 255};
static const Color TEXT_WHITE = {255, 255, 255, 255};
static const Color TEXT_GRAY = {160, 160, 160, 255};
static const Color TEXT_SHADOW = {60, 60, 60, 255};
static const Color TITLE_YELLOW = {255, 255, 0, 255};
static const Color TITLE_SHADOW = {60, 60, 0, 255};
static const Color DIRT_LIGHT = {139, 90, 43, 255};
static const Color DIRT_DARK = {101, 67, 33, 255};

void MainMenuUI::init() {
    // Try to load textures
    if (FileExists("../assets/textures/dirt.png")) {
        dirtTexture = LoadTexture("../assets/textures/dirt.png");
        texturesLoaded = true;
    }

    if (FileExists("../assets/textures/logo.png")) {
        logoTexture = LoadTexture("../assets/textures/logo.png");
    }

    // Reset state
    currentState = MENU_MAIN;
    memset(seedText, 0, sizeof(seedText));
    seedInputActive = true;
    titleBob = 0.0f;
    splashScale = 1.0f;
    splashScaleDir = 1.0f;
}

void MainMenuUI::unload() {
    if (dirtTexture.id > 0) {
        UnloadTexture(dirtTexture);
        dirtTexture = {0};
    }

    if (logoTexture.id > 0) {
        UnloadTexture(logoTexture);
        logoTexture = {0};
    }

    texturesLoaded = false;
}

void MainMenuUI::draw() {
    drawBackground();

    switch (currentState) {
        case MENU_MAIN:
            drawMainMenu();
            break;
        case MENU_WORLD_SELECT:
            drawWorldSelect();
            break;
        case MENU_SEED_INPUT:
            drawSeedInput();
            break;
        case MENU_OPTIONS:
            drawOptions();
            break;
    }
}

void MainMenuUI::drawBackground() {
    int tileSize = 64;

    if (texturesLoaded && dirtTexture.id > 0) {
        // Draw tiled dirt texture
        for (int y = 0; y < GetScreenHeight(); y += tileSize) {
            for (int x = 0; x < GetScreenWidth(); x += tileSize) {
                DrawTextureEx(dirtTexture, {(float)x, (float)y}, 0.0f,
                              (float)tileSize / dirtTexture.width, WHITE);
            }
        }
    } else {
        // Fallback: procedural dirt pattern
        for (int y = 0; y < GetScreenHeight(); y += tileSize) {
            for (int x = 0; x < GetScreenWidth(); x += tileSize) {
                Color c = ((x / tileSize + y / tileSize) % 2 == 0) ? DIRT_LIGHT : DIRT_DARK;
                DrawRectangle(x, y, tileSize, tileSize, c);

                // Add some noise/texture
                for (int i = 0; i < 8; i++) {
                    int px = x + (i * 7) % tileSize;
                    int py = y + (i * 11) % tileSize;
                    DrawRectangle(px, py, 4, 4, Color{
                        (unsigned char)(c.r + 15),
                        (unsigned char)(c.g + 10),
                        (unsigned char)(c.b + 5),
                        255
                    });
                }
            }
        }
    }

    // Dark overlay
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 120});
}

void MainMenuUI::drawTitle(const char* text, int y) {
    // Update bobbing animation
    titleBob += GetFrameTime() * 2.5f;
    float bob = sinf(titleBob) * 5.0f;

    int fontSize = 64;
    int textWidth = MeasureText(text, fontSize);
    int x = (GetScreenWidth() - textWidth) / 2;
    int drawY = y + (int)bob;

    // Draw shadow
    DrawText(text, x + 4, drawY + 4, fontSize, TITLE_SHADOW);

    // Draw main title
    DrawText(text, x, drawY, fontSize, TITLE_YELLOW);
}

void MainMenuUI::drawSplashText(const char* text, int x, int y, float rotation) {
    // Pulsing animation
    splashScale += splashScaleDir * GetFrameTime() * 0.8f;
    if (splashScale > 1.15f) splashScaleDir = -1.0f;
    if (splashScale < 0.9f) splashScaleDir = 1.0f;

    int fontSize = (int)(18 * splashScale);
    int textWidth = MeasureText(text, fontSize);

    Vector2 origin = {(float)textWidth / 2, (float)fontSize / 2};
    DrawTextPro(GetFontDefault(), text, {(float)x, (float)y}, origin, rotation, (float)fontSize, 1, YELLOW);
}

bool MainMenuUI::drawButton(Rectangle bounds, const char* text, bool enabled) {
    Vector2 mouse = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mouse, bounds) && enabled;
    bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Button background
    Color bgColor = enabled ? (isHovered ? BUTTON_HOVER : BUTTON_NORMAL) : BUTTON_DISABLED;
    DrawRectangleRec(bounds, bgColor);

    // 3D border effect
    // Top and left (light)
    DrawLineEx({bounds.x, bounds.y}, {bounds.x + bounds.width, bounds.y}, 2, BUTTON_BORDER_LIGHT);
    DrawLineEx({bounds.x, bounds.y}, {bounds.x, bounds.y + bounds.height}, 2, BUTTON_BORDER_LIGHT);

    // Bottom and right (dark)
    DrawLineEx({bounds.x, bounds.y + bounds.height}, {bounds.x + bounds.width, bounds.y + bounds.height}, 2, BUTTON_BORDER_DARK);
    DrawLineEx({bounds.x + bounds.width, bounds.y}, {bounds.x + bounds.width, bounds.y + bounds.height}, 2, BUTTON_BORDER_DARK);

    // Text
    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);
    int textX = bounds.x + (bounds.width - textWidth) / 2;
    int textY = bounds.y + (bounds.height - fontSize) / 2;

    Color textColor = enabled ? TEXT_WHITE : TEXT_GRAY;

    // Text shadow
    DrawText(text, textX + 2, textY + 2, fontSize, TEXT_SHADOW);
    DrawText(text, textX, textY, fontSize, textColor);

    return isPressed;
}

bool MainMenuUI::drawTextInput(Rectangle bounds, char* text, int maxLength, bool active) {
    // Background
    DrawRectangleRec(bounds, Color{0, 0, 0, 180});

    // Border
    Color borderColor = active ? TEXT_WHITE : TEXT_GRAY;
    DrawRectangleLinesEx(bounds, 2, borderColor);

    // Text
    int fontSize = 20;
    int textY = bounds.y + (bounds.height - fontSize) / 2;
    DrawText(text, bounds.x + 10, textY, fontSize, TEXT_WHITE);

    // Blinking cursor
    if (active && ((int)(GetTime() * 2) % 2 == 0)) {
        int textWidth = MeasureText(text, fontSize);
        DrawRectangle(bounds.x + 12 + textWidth, textY, 2, fontSize, TEXT_WHITE);
    }

    // Handle keyboard input
    if (active) {
        int key = GetCharPressed();
        while (key > 0) {
            int len = strlen(text);
            if (len < maxLength - 1 && key >= 32 && key <= 126) {
                text[len] = (char)key;
                text[len + 1] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(text);
            if (len > 0) {
                text[len - 1] = '\0';
            }
        }

        return IsKeyPressed(KEY_ENTER);
    }

    return false;
}

int MainMenuUI::hashSeed(const std::string& seed) {
    if (seed.empty()) return 0;

    // Try parsing as integer first
    try {
        return std::stoi(seed);
    } catch (...) {
        // Hash the string
        int hash = 0;
        for (char c : seed) {
            hash = hash * 31 + c;
        }
        return hash;
    }
}

void MainMenuUI::drawMainMenu() {
    // Title
    drawTitle("FAKECRAFT", 80);

    // Splash text
    drawSplashText("Now with blocks!", GetScreenWidth() / 2 + 180, 150, -20.0f);

    // Button layout
    float centerX = GetScreenWidth() / 2.0f;
    float buttonW = 320.0f;
    float buttonH = 45.0f;
    float startY = GetScreenHeight() / 2.0f - 50.0f;
    float spacing = 55.0f;

    // Singleplayer button
    if (drawButton({centerX - buttonW/2, startY, buttonW, buttonH}, "Singleplayer")) {
        currentState = MENU_WORLD_SELECT;
    }

    // Multiplayer button (disabled)
    drawButton({centerX - buttonW/2, startY + spacing, buttonW, buttonH}, "Multiplayer (Coming Soon)", false);

    // Options button
    if (drawButton({centerX - buttonW/2, startY + spacing * 2, buttonW, buttonH}, "Options")) {
        currentState = MENU_OPTIONS;
    }

    // Quit button
    if (drawButton({centerX - buttonW/2, startY + spacing * 3, buttonW, buttonH}, "Quit Game")) {
        // Signal to close window
        Settings::gameStateFlag = GameStates::QUIT;
        WindowShouldClose();
    }

    // Version info
    DrawText("Fakecraft v0.1.0", 10, GetScreenHeight() - 25, 16, TEXT_GRAY);
    DrawText("Not affiliated with Mojang", GetScreenWidth() - 230, GetScreenHeight() - 25, 16, TEXT_GRAY);
}

void MainMenuUI::drawWorldSelect() {
    drawTitle("Select World", 80);

    float centerX = GetScreenWidth() / 2.0f;
    float buttonW = 320.0f;
    float buttonH = 45.0f;
    float startY = GetScreenHeight() / 2.0f - 50.0f;
    float spacing = 55.0f;

    // Create New World
    if (drawButton({centerX - buttonW/2, startY, buttonW, buttonH}, "Create New World")) {
        currentState = MENU_SEED_INPUT;
        memset(seedText, 0, sizeof(seedText));
        seedInputActive = true;
    }

    // Load World (disabled for now)
    drawButton({centerX - buttonW/2, startY + spacing, buttonW, buttonH}, "Load World (Coming Soon)", false);

    // Delete World (disabled for now)
    drawButton({centerX - buttonW/2, startY + spacing * 2, buttonW, buttonH}, "Delete World", false);

    // Back
    if (drawButton({centerX - buttonW/2, startY + spacing * 3, buttonW, buttonH}, "Back")) {
        currentState = MENU_MAIN;
    }
}

void MainMenuUI::drawSeedInput() {
    drawTitle("Create New World", 80);

    float centerX = GetScreenWidth() / 2.0f;
    float inputW = 320.0f;
    float inputH = 40.0f;
    float startY = GetScreenHeight() / 2.0f - 60.0f;

    // Seed label
    const char* label = "World Seed (leave blank for random):";
    int labelW = MeasureText(label, 18);
    DrawText(label, centerX - labelW/2, startY - 35, 18, TEXT_WHITE);

    // Seed input box
    bool enterPressed = drawTextInput({centerX - inputW/2, startY, inputW, inputH}, seedText, 32, seedInputActive);

    // Buttons
    float buttonW = 155.0f;
    float buttonH = 45.0f;
    float buttonY = startY + 70.0f;

    // Create World
    if (drawButton({centerX - buttonW - 5, buttonY, buttonW, buttonH}, "Create World") || enterPressed) {
        std::string seed(seedText);
        int hashedSeed = hashSeed(seed);

        if (hashedSeed == 0) {
            Settings::worldSeed = GetRandomValue(0, 1000000000);
        } else {
            Settings::worldSeed = hashedSeed;
        }

        printf("Starting world with seed: %d\n", Settings::worldSeed);

        DisableCursor();
        ChunkHelper::initNoiseRenderer();
        Settings::previousGameState = Settings::gameStateFlag;
        Settings::gameStateFlag = GameStates::IN_GAME;
        currentState = MENU_MAIN;
    }

    // Cancel
    if (drawButton({centerX + 5, buttonY, buttonW, buttonH}, "Cancel")) {
        currentState = MENU_WORLD_SELECT;
    }
}

void MainMenuUI::drawOptions() {
    drawTitle("Options", 80);

    float centerX = GetScreenWidth() / 2.0f;
    float buttonW = 320.0f;
    float buttonH = 45.0f;
    float startY = GetScreenHeight() / 2.0f - 80.0f;
    float spacing = 55.0f;

    // Render Distance
    char renderText[64];
    snprintf(renderText, sizeof(renderText), "Render Distance: %d chunks", Settings::preLoadDistance);
    if (drawButton({centerX - buttonW/2, startY, buttonW, buttonH}, renderText)) {
        Settings::preLoadDistance++;
        if (Settings::preLoadDistance > 16) {
            Settings::preLoadDistance = 4;
        }
    }

    // FOV
    char fovText[64];
    snprintf(fovText, sizeof(fovText), "FOV: %.0f", Settings::fov);
    if (drawButton({centerX - buttonW/2, startY + spacing, buttonW, buttonH}, fovText)) {
        Settings::fov += 10.0f;
        if (Settings::fov > 110.0f) {
            Settings::fov = 60.0f;
        }
    }

    // Fullscreen toggle
    const char* fullscreenText = IsWindowFullscreen() ? "Fullscreen: ON" : "Fullscreen: OFF";
    if (drawButton({centerX - buttonW/2, startY + spacing * 2, buttonW, buttonH}, fullscreenText)) {
        ToggleFullscreen();
    }

    // Done
    if (drawButton({centerX - buttonW/2, startY + spacing * 3, buttonW, buttonH}, "Done")) {
        currentState = MENU_MAIN;
    }
}