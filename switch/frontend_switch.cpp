// Switch replacements for the recompui calls of the PC frontend; the Switch options live in the NX Options menu
// (nx_settings.cpp).
#include <cstdio>

#include <SDL.h>
#include <switch.h>

#include "recomp.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"

extern SDL_Window* cvnx_window;
extern const char* cvnx_game_title;  // main_switch.cpp

void recompui::message_box(const char* msg) {
    fprintf(stderr, "[CVNX] %s\n", msg);
    ErrorApplicationConfig config;
    errorApplicationCreate(&config, cvnx_game_title, msg);
    errorApplicationShow(&config);
}

void recompui::get_window_size(int& width, int& height) {
    if (cvnx_window == nullptr) {
        width = 1280;
        height = 720;
        return;
    }
    SDL_GetWindowSize(cvnx_window, &width, &height);
}

bool recompui::renderer::RT64HighPrecisionFBEnabled() {
    return false;
}

// Pumps RmlUi callbacks registered by mods; there is no RmlUi on Switch.
extern "C" void recomp_run_ui_callbacks(uint8_t*, recomp_context*) {}

// The PC port's patches print debug text every frame through recomp_puts, one character at a time. On Switch stdout
// is the unbuffered log on the SD card, so that was hundreds of SD writes per frame on the game thread. The game's
// own text goes nowhere here; the patches' print.c calls it.
extern "C" void recomp_puts(uint8_t*, recomp_context*) {}
