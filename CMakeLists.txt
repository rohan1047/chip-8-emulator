cmake_minimum_required(VERSION 3.10)
project(chip8)
set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

find_package(SDL2 REQUIRED)

add_executable(chip8 src/main.cpp src/chip8.cpp)

target_include_directories(chip8 PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(chip8 PRIVATE ${SDL2_LIBRARIES})