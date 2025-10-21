#ifndef CHIP8_H
#define CHIP8_H

#include <array>
#include <cstdint>
#include <chrono>

class Chip8 {
public:
    Chip8();
    void initialize();
    void loadROM(const char* filename);
    void cycle();
    uint16_t pc;
    uint16_t opcode;
    uint16_t I;
    uint16_t sp;
    std::array<uint8_t, 64 * 32> gfx;
    bool drawFlag;
    std::array<uint8_t, 4096> memory;
    std::array<uint8_t, 16> V;
    std::array<uint16_t, 16> stack;
    uint8_t delay_timer;
    uint8_t sound_timer;
    std::array<uint8_t, 16> key;
    size_t romSize;

private:
    void updateTimers();
    std::chrono::high_resolution_clock::time_point lastTimerUpdate;
};

#endif