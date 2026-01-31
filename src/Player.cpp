//
// Created by Tristan on 1/30/26.
//

#include "../include/Player.hpp"

Player::Player() {
    this->camera = { 0 };
    this->camera.position = (Vector3){ 0.0f, 0.0f, 10.0f };  // Camera position
    this->camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    this->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    this->camera.fovy = 45;                                // Camera field-of-view Y
    this->camera.projection = CAMERA_PERSPECTIVE;             // Camera mode type
}

void Player::update() {
    this->move();
}

Camera3D& Player::getCamera() {
    return this->camera;
}

void Player::move() {
    UpdateCameraPro(&camera,
            (Vector3){
                (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))*0.1f -      // Move forward-backward
                (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))*0.1f,
                (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))*0.1f -   // Move right-left
                (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))*0.1f,
                (IsKeyDown(KEY_SPACE)*0.1f - IsKeyDown(KEY_LEFT_SHIFT)*0.2f)                                                // Move up-down
            },
            (Vector3){
                GetMouseDelta().x*0.05f,                            // Rotation: yaw
                GetMouseDelta().y*0.05f,                            // Rotation: pitch
                0.0f                                                // Rotation: roll
            },
            GetMouseWheelMove()*2.0f);                              // Move to target (zoom)
}
