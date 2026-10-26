// Linux test harness for the Switch build: the same game code, runtime patches, GLideN64 renderer context and
// core glue as switch/, run in an SDL window (Xvfb + Mesa is enough) so boot problems can be reproduced without a
// console. Not shipped. Usage: rs64host <rom.z64> [seconds]
// Env: RS64H_SHOT_MS=<ms> saves a screenshot every <ms> to shots/; RS64H_START_MS=a,b,... pulses Start at those
// times; RS64H_PAD=<ms>:<buttons hex>:<len ms>,... holds N64 buttons.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>

#include "audio/audio.h"
#include "game/hook_helpers.h"
#include "librecomp/game.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/rsp.hpp"
#include "nx_settings.h"
#include "recompinput/recompinput.h"
#include "recompui/recompui.h"
#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

SDL_Window* cvnx_window = nullptr;
const char* cvnx_game_title = "Rogue Squadron 64 Recompiled (host)";
const char* cvnx_mod_game_id = "rs64";

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();
std::unique_ptr<ultramodern::renderer::RendererContext> cvnx_create_render_context(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode);
extern "C" void cvnx_window_set_size(int, int) {}
void rs64_register_overlays();
const char* const* rs64_register_patches();
void rs64_register_mod_events(const char* const* patch_events);
extern "C" volatile uint8_t* volatile g_recomp_rdram_for_wp_raw;
extern "C" void rs64_asset_overrides_init();
extern "C" void rs64_camera_shots_init();
extern "C" void rs64_mod_levels_init();
extern "C" void rs64_menu_config_init(void);

extern "C" void rs64_entrypoint_with_rdram_capture(uint8_t* rdram, recomp_context* ctx) {
    g_recomp_rdram_for_wp_raw = rdram;
    rs64_asset_overrides_init();
    rs64_camera_shots_init();
    rs64_mod_levels_init();
    recomp_entrypoint(rdram, ctx);
}

static const auto g_boot = std::chrono::steady_clock::now();
extern "C" unsigned long long cvnx_ms_since_boot() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_boot).count();
}

namespace {
    std::u8string GAME_ID = u8"rs64.n64.us.1.0";

    ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
        SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "1");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
            fprintf(stderr, "[HOST] SDL_Init failed: %s\n", SDL_GetError());
        }
        return {};
    }

    ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
        cvnx_window = SDL_CreateWindow(cvnx_game_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       nx::screen_width(), nx::screen_height(), SDL_WINDOW_OPENGL);
        if (cvnx_window == nullptr) fprintf(stderr, "[HOST] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return cvnx_window;
    }

    void update_gfx(void*) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) ultramodern::quit();
        }
    }

    std::string get_game_thread_name(const OSThread* t) { return "Game Thread " + std::to_string(t->id); }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom.z64> [seconds]\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 60;
    const std::filesystem::path data_dir = std::filesystem::current_path() / "host_data";
    std::filesystem::create_directories(data_dir);
    nx::load();
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    SDL_InitSubSystem(SDL_INIT_AUDIO);

    recomp::register_config_path(data_dir);
    recomp::GameEntry game{
        .rom_hash = 0x6B66A44153594DEAULL,
        .internal_name = "ROGUE SQUADRON",
        .game_id = GAME_ID,
        .mod_game_id = "rs64",
        .save_type = recomp::SaveType::Eep4k,
        .is_enabled = true,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = rs64_entrypoint_with_rdram_capture,
    };
    rs64_register_overlays();
    rs64_register_mod_events(rs64_register_patches());
    recomp::register_game(game);
    rs64_menu_config_init();
    recomp::check_all_stored_roms();
    if (!recomp::is_rom_valid(GAME_ID)) {
        auto r = recomp::select_rom(argv[1], GAME_ID);
        fprintf(stderr, "[HOST] ROM import -> %d\n", (int)r);
    }

    std::thread([seconds] {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        fprintf(stderr, "[HOST] time limit reached, exiting\n");
        std::_Exit(0);
    }).detach();

    recomp::start(recomp::Configuration{
        .project_version = {0, 1, 0},
        .window_handle = {},
        .rsp_callbacks = {.get_rsp_microcode = rs64::audio::get_rsp_microcode},
        .renderer_callbacks = {.create_render_context = cvnx_create_render_context},
        .audio_callbacks = {
            .queue_samples = rs64::audio::queue_samples,
            .get_frames_remaining = rs64::audio::get_frames_remaining,
            .set_frequency = rs64::audio::set_frequency,
        },
        .input_callbacks = {
            .poll_input = recompinput::poll_inputs,
            .get_input = [](int c, uint16_t* b, float* x, float* y) -> bool {
                extern bool cvnx_get_input(int, uint16_t*, float*, float*);
                return cvnx_get_input(c, b, x, y);
            },
            .set_rumble = recompinput::set_rumble,
            .get_connected_device_info = [](int c) {
                return c == 0 ? ultramodern::input::connected_device_info_t{ultramodern::input::Device::Controller, ultramodern::input::Pak::RumblePak}
                              : ultramodern::input::connected_device_info_t{ultramodern::input::Device::None, ultramodern::input::Pak::None};
            },
        },
        .gfx_callbacks = {.create_gfx = create_gfx, .create_window = create_window, .update_gfx = update_gfx},
        .events_callbacks = {
            .vi_callback = [] { g_vi_tick = g_vi_tick + 1; },
            .gfx_init_callback = [] {
                if (recomp::is_rom_valid(GAME_ID)) recomp::start_game(GAME_ID);
            },
        },
        .error_handling_callbacks = {.message_box = recompui::message_box},
        .threads_callbacks = {.get_game_thread_name = get_game_thread_name},
        .message_queue_control = {},
    });
    return 0;
}
