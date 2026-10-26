// Linux harness replacements for the Switch-only pieces: scripted pad, no NX menu (screenshots instead), plain
// message boxes, and the runtime hooks that only exist in __SWITCH__ builds.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL.h>
#include <GL/gl.h>

#include "recomp.h"
#include "recompinput/recompinput.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "ultramodern/input.hpp"

extern SDL_Window* cvnx_window;
extern "C" unsigned long long cvnx_ms_since_boot();

void recompui::message_box(const char* msg) { fprintf(stderr, "[HOST] message box: %s\n", msg); }
void recompui::get_window_size(int& width, int& height) {
    if (cvnx_window) SDL_GetWindowSize(cvnx_window, &width, &height);
    else { width = 1280; height = 720; }
}
bool recompui::renderer::RT64HighPrecisionFBEnabled() { return false; }
extern "C" void recomp_run_ui_callbacks(uint8_t*, recomp_context*) {}
extern "C" void recomp_puts(uint8_t*, recomp_context*) {}

// Switch-only runtime hook (events.cpp under __SWITCH__): never claims here.
extern "C" bool cvnx_claim_screen_update() { return false; }

namespace {
    struct Press { unsigned long long at, len; uint16_t buttons; };
    std::vector<Press> script() {
        std::vector<Press> out;
        if (const char* s = std::getenv("RS64H_START_MS")) {
            for (const char* p = s; *p;) {
                out.push_back({std::strtoull(p, nullptr, 10), 150, 0x1000});
                p = std::strchr(p, ',');
                if (!p) break;
                ++p;
            }
        }
        if (const char* s = std::getenv("RS64H_PAD")) {
            for (const char* p = s; *p;) {
                char* e;
                unsigned long long at = std::strtoull(p, &e, 10);
                uint16_t b = (uint16_t)std::strtoul(e + 1, &e, 16);
                unsigned long long len = std::strtoull(e + 1, &e, 10);
                out.push_back({at, len, b});
                p = std::strchr(e, ',');
                if (!p) break;
                ++p;
            }
        }
        return out;
    }
}

bool cvnx_get_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    static const std::vector<Press> presses = script();
    *buttons = 0;
    *x = 0;
    *y = 0;
    if (controller_num != 0) return false;
    const unsigned long long now = cvnx_ms_since_boot();
    for (const Press& p : presses) {
        if (now >= p.at && now < p.at + p.len) *buttons |= p.buttons;
    }
    return true;
}
void recompinput::poll_inputs() {}
void recompinput::set_rumble(int, bool) {}
void recompinput::update_rumble() {}
bool cvnx_menu_owns_pad() { return false; }

// Called before each swap: a screenshot every RS64H_SHOT_MS into shots/.
extern "C" void cvnx_menu_on_swap(int width, int height) {
    static const unsigned long long every = std::getenv("RS64H_SHOT_MS") ? std::strtoull(std::getenv("RS64H_SHOT_MS"), nullptr, 10) : 0;
    static unsigned long long next = 0;
    if (every == 0) return;
    const unsigned long long now = cvnx_ms_since_boot();
    if (now < next) return;
    next = now + every;
    std::filesystem::create_directories("shots");
    std::vector<uint8_t> px((size_t)width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    char name[64];
    std::snprintf(name, sizeof(name), "shots/%06llu.ppm", now);
    if (FILE* f = std::fopen(name, "wb")) {
        std::fprintf(f, "P6\n%d %d\n255\n", width, height);
        for (int row = height - 1; row >= 0; row--) std::fwrite(px.data() + (size_t)row * width * 3, 1, (size_t)width * 3, f);
        std::fclose(f);
    }
}
