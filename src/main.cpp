#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>
#include "chip8.h"

const int WINDOW_WIDTH = 64 * 10;
const int WINDOW_HEIGHT = 32 * 10;
const int SCALE = 10;

bool soundPlaying = false;

void audioCallback(void* userdata, Uint8* stream, int len) {
    static int amplitude = 28000;
    static int frequency = 440;
    static int sampleRate = 44100;
    static double phase = 0.0;

    Sint16* buffer = (Sint16*)stream;
    int length = len / 2;

    for (int i = 0; i < length; i++) {
        buffer[i] = (Sint16)(amplitude * sin(phase));
        phase += 2.0 * M_PI * frequency / sampleRate;
        if (phase > 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path to CHIP-8 ROM>" << std::endl;
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audioCallback;

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (dev == 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
    }

    SDL_PauseAudioDevice(dev, 1);

    SDL_Window* window = SDL_CreateWindow("CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    Chip8 chip8;
    chip8.loadROM(argv[1]);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                uint8_t value = (event.type == SDL_KEYDOWN) ? 1 : 0;
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8.key[0x1] = value; break;
                    case SDLK_2: chip8.key[0x2] = value; break;
                    case SDLK_3: chip8.key[0x3] = value; break;
                    case SDLK_4: chip8.key[0xC] = value; break;
                    case SDLK_q: chip8.key[0x4] = value; break;
                    case SDLK_a: chip8.key[0x7] = value; break;
                    case SDLK_w: chip8.key[0x5] = value; break;
                    case SDLK_e: chip8.key[0x6] = value; break;
                    case SDLK_r: chip8.key[0xD] = value; break;
                    case SDLK_s: chip8.key[0x8] = value; break;
                    case SDLK_d: chip8.key[0x9] = value; break;
                    case SDLK_f: chip8.key[0xE] = value; break;
                    case SDLK_z: chip8.key[0xA] = value; break;
                    case SDLK_x: chip8.key[0x0] = value; break;
                    case SDLK_c: chip8.key[0xB] = value; break;
                    case SDLK_v: chip8.key[0xF] = value; break;
                    case SDLK_ESCAPE: running = false; break;
                    default: break;
                }
            }
        }

        chip8.cycle();

        if (chip8.drawFlag) {
            chip8.drawFlag = false;

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (int y = 0; y < 32; ++y) {
                for (int x = 0; x < 64; ++x) {
                    if (chip8.gfx[y * 64 + x]) {
                        SDL_Rect rect = { x * SCALE, y * SCALE, SCALE, SCALE };
                        SDL_RenderFillRect(renderer, &rect);
                    }
                }
            }

            SDL_RenderPresent(renderer);
        }
        if (chip8.sound_timer > 0) {
            if (!soundPlaying) {
                SDL_PauseAudioDevice(dev, 0);
                soundPlaying = true;
            }
        }
        else {
            if (soundPlaying) {
                SDL_PauseAudioDevice(dev, 1);
                soundPlaying = false;
            }
        }
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}