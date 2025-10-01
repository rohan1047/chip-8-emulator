#include "chip8.h"
#include <fstream>
#include <iostream>
#include <cstring>

const unsigned char chip8_fontset[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80 
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

    for (int i = 0; i < 80; ++i) {
        memory[0x50 + i] = chip8_fontset[i];
    }
    delay_timer = 0;
    sound_timer = 0;
    for (int i = 0; i < 16; ++i) {
        key[i] = 0;
    }
    drawFlag = false;
    romSize = 0;
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

    std::cerr << "ROM loaded: " << size << " bytes" << std::endl;
    std::cerr << "First bytes: ";
    for (int i = 0; i < 10 && i < size; i++) {
        std::cerr << std::hex << (int)memory[0x200 + i] << " ";
    }
    std::cerr << std::endl;
}

void Chip8::cycle() {
    static bool printedMemory = false;
    if (!printedMemory) {
        std::cerr << "Memory near 0x228: ";
        for (int i = 0x228; i < 0x238; ++i) {
            std::cerr << std::hex << (int)memory[i] << " ";
        }
        std::cerr << std::endl;
        printedMemory = true;
    }

    if (pc >= (0x200 + romSize)) {
        std::cerr << "PC past end of ROM (0x" << std::hex << pc << "), resetting to 0x200" << std::endl;
        pc = 0x200;
        return;
    }

    opcode = memory[pc] << 8 | memory[pc + 1];
    std::cerr << "Opcode: 0x" << std::hex << opcode << " at PC: 0x" << pc << std::endl;

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0:
                    gfx.fill(0);
                    drawFlag = true;
                    pc += 2;
                    break;

                case 0x00EE:
                    if (sp > 0) {
                        sp--;
                        pc = stack[sp];
                    }
                    else {
                        std::cerr << "Stack underflow on 0x00EE opcode" << std::endl;
                        pc += 2;
                    }
                    break;

                default:
                    std::cerr << "Unknown opcode [0x0000]: 0x" << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }
            break;

        case 0x1000:
            pc = opcode & 0x0FFF;
            break;

        case 0x3000: {
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            uint8_t byte = opcode & 0x00FF;
            pc += (V[Vx] == byte) ? 4 : 2;
            break;
        }

        case 0x4000: {
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            uint8_t byte = opcode & 0x00FF;
            pc += (V[Vx] != byte) ? 4 : 2;
            break;
        }

        case 0x6000: {
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            uint8_t byte = opcode & 0x00FF;
            V[Vx] = byte;
            pc += 2;
            break;
        }

        case 0x7000: {
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            uint8_t byte = opcode & 0x00FF;
            V[Vx] += byte;
            pc += 2;
            break;
        }

        case 0x8000: {
            uint8_t Vx = (opcode & 0x0F00) >> 8;
            uint8_t Vy = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0:
                    V[Vx] = V[Vy];
                    break;
                case 0x1:
                    V[Vx] |= V[Vy];
                    break;
                case 0x2:
                    V[Vx] &= V[Vy];
                    break;
                case 0x3:
                    V[Vx] ^= V[Vy];
                    break;
                case 0x4: {
                    uint16_t sum = V[Vx] + V[Vy];
                    V[0xF] = (sum > 255) ? 1 : 0;
                    V[Vx] = sum & 0xFF;
                    break;
                }
                case 0x5: {
                    V[0xF] = (V[Vx] >= V[Vy]) ? 1 : 0;
                    V[Vx] -= V[Vy];
                    break;
                }
                case 0x6:
                    V[0xF] = V[Vx] & 0x1;
                    V[Vx] >>= 1;
                    break;
                case 0x7: {
                    V[0xF] = (V[Vy] >= V[Vx]) ? 1 : 0;
                    V[Vx] = V[Vy] - V[Vx];
                    break;
                }
                case 0xE:
                    V[0xF] = (V[Vx] & 0x80) >> 7;
                    V[Vx] <<= 1;
                    break;
                default:
                    std::cerr << "Unknown opcode [0x8000]: 0x" << std::hex << opcode << std::endl;
                    break;
            }
            pc += 2;
            break;
        }

        case 0xA000: {
            uint16_t address = opcode & 0x0FFF;
            I = address;
            pc += 2;
            break;
        }

        case 0xD000: {
            uint8_t x = V[(opcode & 0x0F00) >> 8];
            uint8_t y = V[(opcode & 0x00F0) >> 4];
            uint8_t height = opcode & 0x000F;

            V[0xF] = 0;

            for (int row = 0; row < height; row++) {
                uint8_t spriteByte = memory[I + row];
                for (int col = 0; col < 8; col++) {
                    bool spritePixel = (spriteByte & (0x80 >> col)) != 0;
                    size_t gfxIndex = ((y + row) % 32) * 64 + ((x + col) % 64);
                    if (spritePixel) {
                        if (gfx[gfxIndex] == 1) {
                            V[0xF] = 1;
                        }
                        gfx[gfxIndex] ^= 1;
                    }
                }
            }

            drawFlag = true;
            pc += 2;
            break;
        }

        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x9E: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    pc += (key[V[Vx]] != 0) ? 4 : 2;
                    break;
                }
                case 0xA1: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    pc += (key[V[Vx]] == 0) ? 4 : 2;
                    break;
                }
                default:
                    std::cerr << "Unknown opcode [0xE000]: 0x" << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }
            break;

        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x07:
                    V[(opcode & 0x0F00) >> 8] = delay_timer;
                    pc += 2;
                    break;
                case 0x0A: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    bool keyPressed = false;
                    for (int i = 0; i < 16; ++i) {
                        if (key[i] != 0) {
                            V[Vx] = i;
                            keyPressed = true;
                            break;
                        }
                    }
                    if (!keyPressed) return;
                    pc += 2;
                    break;
                }
                case 0x15:
                    delay_timer = V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;
                case 0x18:
                    sound_timer = V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;
                case 0x1E:
                    I += V[(opcode & 0x0F00) >> 8];
                    pc += 2;
                    break;
                case 0x29:
                    I = 0x50 + (V[(opcode & 0x0F00) >> 8] * 5);
                    pc += 2;
                    break;
                case 0x33: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    uint8_t value = V[Vx];
                    memory[I]     = value / 100;
                    memory[I + 1] = (value / 10) % 10;
                    memory[I + 2] = value % 10;
                    pc += 2;
                    break;
                }
                case 0x55: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    for (int i = 0; i <= Vx; ++i) {
                        memory[I + i] = V[i];
                    }
                    pc += 2;
                    break;
                }
                case 0x65: {
                    uint8_t Vx = (opcode & 0x0F00) >> 8;
                    for (int i = 0; i <= Vx; ++i) {
                        V[i] = memory[I + i];
                    }
                    pc += 2;
                    break;
                }
                default:
                    std::cerr << "Unknown opcode [0xF000]: 0x" << std::hex << opcode << std::endl;
                    pc += 2;
                    break;
            }
            break;

        default:
            std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            pc += 2;
            break;
    }

    if (delay_timer > 0) delay_timer--;
    if (sound_timer > 0) sound_timer--;

    if (pc > 0xFFF) {
        std::cerr << "PC went out of range at cycle end, resetting to 0x200" << std::endl;
        pc = 0x200;
    }

    std::cerr << "PC after cycle: 0x" << std::hex << pc << std::endl;
}