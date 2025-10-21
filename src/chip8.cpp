#include "chip8.h"
#include <fstream>
#include <iostream>
#include <random>
#include <cstring>
#include <chrono>

const unsigned char chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

Chip8::Chip8() {
    initialize();
}

void Chip8::initialize() {
    pc = 0x200;
    opcode = 0;
    I = 0;
    sp = 0;
    gfx.fill(0);
    stack.fill(0);
    V.fill(0);
    memory.fill(0);
    key.fill(0);
    drawFlag = false;
    romSize = 0;
    delay_timer = 0;
    sound_timer = 0;
    std::memcpy(&memory[0x50], chip8_fontset, sizeof(chip8_fontset));
    lastTimerUpdate = std::chrono::high_resolution_clock::now();
}

void Chip8::loadROM(const char* filename) {
    std::ifstream rom(filename, std::ios::binary | std::ios::ate);
    if (!rom) {
        std::cerr << "Failed to open ROM: " << filename << std::endl;
        return;
    }
    std::streamsize size = rom.tellg();
    rom.seekg(0, std::ios::beg);
    if (size > (4096 - 512)) {
        std::cerr << "ROM too large to fit in memory" << std::endl;
        return;
    }
    if (!rom.read(reinterpret_cast<char*>(&memory[0x200]), size)) {
        std::cerr << "Failed to read ROM" << std::endl;
        return;
    }
    romSize = size;
    std::cerr << "ROM loaded: " << size << " bytes\n";
}

void Chip8::updateTimers() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(now - lastTimerUpdate).count();
    if (duration >= 16) {
        if (delay_timer > 0) --delay_timer;
        if (sound_timer > 0) --sound_timer;
        lastTimerUpdate = now;
    }
}

void Chip8::cycle() {
    if (pc + 1 >= 4096) {
        std::cerr << "PC out of bounds\n";
        pc = 0x200;
        return;
    }
    opcode = memory[pc] << 8 | memory[pc + 1];
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;
    uint8_t n = opcode & 0x000F;
    switch (opcode & 0xF000) {
        case 0x0000:
            if (kk == 0xE0) { gfx.fill(0); drawFlag = true; pc += 2; }
            else if (kk == 0xEE) { pc = (sp > 0) ? stack[--sp] : (pc + 2); }
            else { std::cerr << "Unknown opcode: " << std::hex << opcode << "\n"; pc += 2; }
            break;
        case 0x1000: pc = nnn; break;
        case 0x2000: stack[sp++] = pc + 2; pc = nnn; break;
        case 0x3000: pc += (V[x] == kk) ? 4 : 2; break;
        case 0x4000: pc += (V[x] != kk) ? 4 : 2; break;
        case 0x5000: pc += (V[x] == V[y]) ? 4 : 2; break;
        case 0x6000: V[x] = kk; pc += 2; break;
        case 0x7000: V[x] += kk; pc += 2; break;
        case 0x8000:
            switch (n) {
                case 0x0: V[x] = V[y]; break;
                case 0x1: V[x] |= V[y]; break;
                case 0x2: V[x] &= V[y]; break;
                case 0x3: V[x] ^= V[y]; break;
                case 0x4: {
                    uint16_t sum = V[x] + V[y];
                    V[0xF] = (sum > 255);
                    V[x] = sum & 0xFF;
                    break;
                }
                case 0x5: V[0xF] = (V[x] >= V[y]); V[x] -= V[y]; break;
                case 0x6: V[0xF] = V[x] & 0x1; V[x] >>= 1; break;
                case 0x7: V[0xF] = (V[y] >= V[x]); V[x] = V[y] - V[x]; break;
                case 0xE: V[0xF] = (V[x] & 0x80) >> 7; V[x] <<= 1; break;
                default: std::cerr << "Unknown opcode: " << std::hex << opcode << "\n"; break;
            }
            pc += 2;
            break;
        case 0x9000: pc += (V[x] != V[y]) ? 4 : 2; break;
        case 0xA000: I = nnn; pc += 2; break;
        case 0xB000: pc = nnn + V[0]; break;
        case 0xC000: {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint8_t> dis(0, 255);
            V[x] = dis(gen) & kk;
            pc += 2;
            break;
        }
        case 0xD000: {
            uint8_t height = n;
            V[0xF] = 0;
            for (int row = 0; row < height; ++row) {
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; ++col) {
                    if ((sprite & (0x80 >> col)) != 0) {
                        size_t idx = ((V[y] + row) % 32) * 64 + ((V[x] + col) % 64);
                        if (gfx[idx] == 1) V[0xF] = 1;
                        gfx[idx] ^= 1;
                    }
                }
            }
            drawFlag = true;
            pc += 2;
            break;
        }
        case 0xE000:
            if (kk == 0x9E) pc += (key[V[x]] != 0) ? 4 : 2;
            else if (kk == 0xA1) pc += (key[V[x]] == 0) ? 4 : 2;
            else { std::cerr << "Unknown opcode: " << std::hex << opcode << "\n"; pc += 2; }
            break;
        case 0xF000:
            switch (kk) {
                case 0x07: V[x] = delay_timer; pc += 2; break;
                case 0x0A: {
                    bool pressed = false;
                    for (uint8_t i = 0; i < 16; ++i) {
                        if (key[i] != 0) { V[x] = i; pressed = true; break; }
                    }
                    if (!pressed) return; 
                    pc += 2; 
                    break;
                }
                case 0x15: delay_timer = V[x]; pc += 2; break;
                case 0x18: sound_timer = V[x]; pc += 2; break;
                case 0x1E: I += V[x]; pc += 2; break;
                case 0x29: I = 0x50 + V[x] * 5; pc += 2; break;
                case 0x33: memory[I] = V[x] / 100; memory[I + 1] = (V[x] / 10) % 10; memory[I + 2] = V[x] % 10; pc += 2; break;
                case 0x55: for (int i = 0; i <= x; ++i) memory[I + i] = V[i]; pc += 2; break;
                case 0x65: for (int i = 0; i <= x; ++i) V[i] = memory[I + i]; pc += 2; break;
                default: std::cerr << "Unknown opcode: " << std::hex << opcode << "\n"; pc += 2; break;
            }
            break;
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
            pc += 2;
            break;
    }
    updateTimers();
}