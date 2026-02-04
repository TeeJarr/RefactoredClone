cmake_minimum_required(VERSION 3.20)
project(RefactoredClone)

set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCES src/*.cpp)

find_package(raylib)

add_executable(${PROJECT_NAME} ${SOURCES}
        src/World/Biome/Biome.cpp
        src/World/Biome/Biome.hpp
        src/World/Region/Region.cpp
        src/World/Region/Region.hpp)
target_link_libraries(${PROJECT_NAME} PUBLIC raylib)
target_include_directories(${PROJECT_NAME} PUBLIC include src include/Block src/World src/Engine src/Menu)
