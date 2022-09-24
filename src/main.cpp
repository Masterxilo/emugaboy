#include "gameboy/emulator/cartridge.h"
#include "gameboy/emulator/cpu.h"
#include "gameboy/emulator/gpu.h"
#include "gameboy/emulator/mmu.h"
#include "gameboy/emulator/dma.h"
#include "gameboy/emulator/joypad.h"
#include "gameboy/emulator/timer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

void my_atexit() {
    printf("my_atexit FIRES!\n");
}

static int SDLCALL my_event_filter(void *userdata, SDL_Event * event)
{
    if (event->type == SDL_QUIT) {
        printf("SDL_QUIT received!\n");
        // hard exit
        exit(0);
    }
    return 1;  // let all events be added to the queue since we always return 1.
}

#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char *argv[])
{
    printf("emugaboy starts, loading cartridge...\n");
    auto cartridge{gameboy::emulator::Cartridge::load("res/roms/SuperMarioLand.gb")};
    printf("cartridge loaded.\n");
    gameboy::emulator::CPU cpu;
    gameboy::emulator::GPU gpu;
    unsigned char wram[0x2000], hram[0x7F];
    gameboy::emulator::MMU mmu;
    gameboy::emulator::DMA dma{&gpu, &mmu};
    gameboy::emulator::Joypad joypad;
    gameboy::emulator::Timer timer;
    gameboy::emulator::MMU::MemPointers mem_pointers;
    mem_pointers.cartridge = &cartridge;
    mem_pointers.gpu       = &gpu;
    mem_pointers.cpu       = &cpu;
    mem_pointers.dma       = &dma;
    mem_pointers.joypad    = &joypad;
    mem_pointers.timer     = &timer;
    mem_pointers.wram = wram;
    mem_pointers.hram = hram;
    mmu.set_mem_pointers(mem_pointers);

    ::SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    SDL_Window* const window = ::SDL_CreateWindow("Emugaboy",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * 4, 144 * 4, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* const renderer = ::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* const texture = ::SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, 160, 144);
    constexpr unsigned seconds_per_update = 1000 / 60, insts_per_update = 1048576 / 60;
    unsigned acc_update_time = 0;
    Uint32 previous_time = ::SDL_GetTicks();

    // optional nonblocking STDIN [[
#ifndef _WIN32
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
    //]]
    // exitable [[
    atexit(my_atexit);
    SDL_SetEventFilter(my_event_filter, nullptr);
    // ]]

    for (bool running = true; running;)
    {
        // optional nonblocking STDIN [[
#ifndef _WIN32
        char buf[200];
        int numRead = read(0, buf, sizeof(buf));
        if (numRead > 0) {
            printf("stdin: '%s' %d\n", buf, buf[0]);
        }
#endif
        // ]]

        ::SDL_PumpEvents();

        const Uint8* const keyboard_state = ::SDL_GetKeyboardState(nullptr);
        for (int i = 0; i < 128; i++) {
            //putc(keyboard_state[i] ? 'x' : '.', stdout);
        }
        //printf("\n");

        // quit using esc
        if (keyboard_state[SDL_SCANCODE_ESCAPE])
            running = false;

        // keyboard control/button states
        const unsigned keys_mask = keyboard_state[SDL_SCANCODE_D] << 0 | // arrows = WASD
                                   keyboard_state[SDL_SCANCODE_A] << 1 |
                                   keyboard_state[SDL_SCANCODE_W] << 2 |
                                   keyboard_state[SDL_SCANCODE_S] << 3 |

                                    // note: did not use backspace/enter but these keys!!!
                                   keyboard_state[SDL_SCANCODE_J] << 4 | // ab
                                   keyboard_state[SDL_SCANCODE_I] << 5 | // ab
                                   keyboard_state[SDL_SCANCODE_E] << 6 | // select/start
                                   keyboard_state[SDL_SCANCODE_Q] << 7 // select/start
                                   ;
        
        //printf("keys_mask = %#02x\n", keys_mask);
        joypad.push_key_states(keys_mask);

        // Make screenshots using F
        static bool triggered = false;
             if (keyboard_state[SDL_SCANCODE_F]) triggered = true;
        else if (triggered)
        {
            std::ofstream stream{"res/images/screenshot.ppm", std::ios::out | std::ios::binary};
            stream << "P6\n" << 160 << ' ' << 144 << '\n' << 255 << '\n';
            for (int i = 0; i < 160 * 144; ++i)
            {
                const unsigned char px = 0xFF * (3 - gpu.get_framebuffer_pixel(i)) / 3;
                const unsigned char color[]{px, px, px};
                stream.write(reinterpret_cast<const char*>(color), 3);
            }
            triggered = false;
        }

        // MAIN SYSTEM STATE UPDATE LOOP
        const Uint32 current_time = ::SDL_GetTicks();
        acc_update_time += current_time - previous_time;
        previous_time = current_time;
        // update the system for the actual elapsed amount of time ... 
        // TODO ?! this assumes that each instruction takes 1 cycle/the same amount of cycles no?
        //
        // TODO this bad updating strategy explains why we have bands across the scrren when walking right!
        /*for (; acc_update_time >= seconds_per_update; acc_update_time -= seconds_per_update)
        {
            static unsigned i = 0;
            while (i < insts_per_update)
            {
                const unsigned cycles = cpu.next_step(mmu); i += cycles;
                dma.tick(cycles);

                unsigned interrupts = 0;
                interrupts      |= timer.tick(cycles);
                interrupts      |=   gpu.tick(cycles);

                cpu.request_interrupts(interrupts);
            }
            i -= insts_per_update;
        }*/

        // just execute random amount of instructions,
        // almost as good... (except we have no idea about the SDL timing...)
        // but at least like this, the tearing will be less consistent/inconsistent...
        // we will be redrawing the screen a ton of times...
        for (int i = 0; i < 600 /* 300 */; i++) {
                const unsigned cycles = cpu.next_step(mmu); i += cycles;
                dma.tick(cycles);

                unsigned interrupts = 0;
                interrupts      |= timer.tick(cycles);
                interrupts      |=   gpu.tick(cycles);

                cpu.request_interrupts(interrupts);
        }

        // display current pixels
        Uint32* pixels;
        int pitch;

        ::SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

        // translate all pixels (2 bits per pixel, value 0-3) to grayscale rgb 0 to 255 (0xff)
        for (int i = 0; i < 160 * 144; ++i)
        {
            const unsigned px = 0xFF * (3 - gpu.get_framebuffer_pixel(i)) / 3;
            pixels[i] = 0xFF000000 | px | px << 8 | px << 16;
        }

        ::SDL_UnlockTexture(texture);

        ::SDL_RenderClear(renderer);
        ::SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        ::SDL_RenderPresent(renderer);
    }
    ::SDL_DestroyTexture(texture);
    ::SDL_DestroyRenderer(renderer);
    ::SDL_DestroyWindow(window);
    ::SDL_Quit();

    return 0;
}

