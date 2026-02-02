cmake_minimum_required(VERSION 4.1.2)
project(RefactoredClone)

set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCES src/*.cpp)

find_package(raylib)

add_executable(${PROJECT_NAME} ${SOURCES}
        src/main.cpp
        src/Engine.cpp
        include/Engine.hpp
        src/Player.cpp
        include/Player.hpp
        src/Renderer.cpp
        include/Renderer.hpp
        include/Blocks.hpp
        src/Chunk.cpp
        include/Chunk.hpp
        include/Settings.hpp
        src/Settings.cpp
        src/Menu.cpp
        include/Menu.hpp
        src/LightingSystem.cpp
        src/LightingSystem.hpp
        src/MeshThreadPool.cpp
        src/MeshThreadPool.hpp
        include/Collision.hpp
)
target_link_libraries(${PROJECT_NAME} PUBLIC raylib)
target_include_directories(${PROJECT_NAME} PUBLIC include)
