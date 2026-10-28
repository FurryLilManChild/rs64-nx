// Switch entry point for Rogue Squadron 64 Recompiled. Mirrors src/main/main.cpp without the PC frontend
// (RT64, ImGui panels, touch, file dialogs): the game boots straight from the ROM placed in sdmc:/switch/rs64recomp/.
// Based on DK64Recomp-NX by Trinalord777 (switch/main_switch.cpp).
#include <algorithm>
#include <atomic>
#include <thread>
#include <array>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <exception>
#include <vector>

#include <unistd.h>

#include <SDL.h>
#include <switch.h>

#include "audio/audio.h"
#include "game/hook_helpers.h"
#include "game/upstream_compat.h"
#include "util/debug_logs.h"
#include "nx_cheats.h"
#include "nx_settings.h"
#include "librecomp/game.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/rsp.hpp"
#include "recompinput/recompinput.h"
#include "recompui/recompui.h"
#include "ultramodern/config.hpp"
#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

extern "C" {
    extern std::atomic<uint32_t> cvnx_hb_gfx_submit, cvnx_hb_gfx_parsed, cvnx_hb_sp_submit, cvnx_hb_sp_done, cvnx_hb_vi, cvnx_hb_screen;
    extern std::atomic<int> cvnx_gfx_stage;
}


SDL_Window* cvnx_window = nullptr;
const char* cvnx_game_title = "Rogue Squadron 64 Recompiled";
const char* cvnx_mod_game_id = "rs64";

// nvdrv keeps the bookkeeping of every GPU allocation (mesa makes one nvMapCreate per texture) in the transfer memory
// libnx hands it at startup: 8 MiB by default (a weak symbol). Measured on the console, uploads start failing with
// GL_OUT_OF_MEMORY near 5,000 live textures; GLideN64 caches up to 8,000, so a long session ended in black and noisy
// textures until a restart. 32 MiB holds about 20,000 allocations.
extern "C" u32 __nx_nv_transfermem_size;
u32 __nx_nv_transfermem_size = 32 * 1024 * 1024;

void cvnx_input_handle_event(const SDL_Event& event);
void cvnx_perf_start();
void cvnx_perf_update();
void cvnx_perf_stop();
extern "C" void cvnx_jit_close();
extern "C" void cvnx_window_set_size(int width, int height);  // window_switch.c
#ifdef CVNX_REMOTE
void cvnx_remote_start();
void cvnx_remote_stop();
extern "C" void cvnx_trace(const char* what);
#endif
bool cvnx_get_input(int controller_num, uint16_t* buttons, float* x, float* y);
ultramodern::input::connected_device_info_t cvnx_get_connected_device_info(int controller_num);
std::unique_ptr<ultramodern::renderer::RendererContext> cvnx_create_render_context(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode);

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();
void rs64_register_overlays();
const char* const* rs64_register_patches();
void rs64_register_mod_events(const char* const* patch_events);
extern "C" volatile uint8_t* volatile g_recomp_rdram_for_wp_raw;
extern "C" void rs64_asset_overrides_init();
extern "C" void rs64_camera_shots_init();
extern "C" void rs64_mod_levels_init();
extern "C" void rs64_menu_config_init(void);

// As src/main/main.cpp: the game's hooks find RDRAM through this global, and the asset tables load first.
extern "C" void rs64_entrypoint_with_rdram_capture(uint8_t* rdram, recomp_context* ctx) {
    g_recomp_rdram_for_wp_raw = rdram;
    rs64_asset_overrides_init();
    rs64_camera_shots_init();
    rs64_mod_levels_init();
    recomp_entrypoint(rdram, ctx);
}

// Milliseconds since the console booted, for the start-up timeline in the log (also core_switch.cpp).
extern "C" unsigned long long cvnx_ms_since_boot() { return armTicksToNs(armGetSystemTick()) / 1000000; }

namespace {
    const std::filesystem::path DATA_DIR = "sdmc:/switch/rs64recomp";
    std::u8string GAME_ID = u8"rs64.n64.us.1.0";
    const std::string VERSION_STRING = "0.1.0";

    // With the time since the console booted, to see where the start of the game goes.
    void step(const char* what) {
        printf("[CVNX] step: %s (%llu ms)\n", what, cvnx_ms_since_boot());
    }

    [[noreturn]] void log_terminate() {
        try {
            if (auto current = std::current_exception()) std::rethrow_exception(current);
            fprintf(stderr, "[CVNX] std::terminate without an active exception\n");
        } catch (const std::exception& e) {
            fprintf(stderr, "[CVNX] uncaught exception: %s\n", e.what());
        } catch (...) {
            fprintf(stderr, "[CVNX] uncaught non-standard exception\n");
        }
        svcSleepThread(2000000000ULL);
        std::abort();
    }

    void init_logging() {
        std::filesystem::create_directories(DATA_DIR);
        socketInitializeDefault();
        if (nxlinkStdio() >= 0) {
            setvbuf(stdout, nullptr, _IONBF, 0);
            setvbuf(stderr, nullptr, _IONBF, 0);
            return;
        }
#ifndef CVNX_REMOTE
        socketExit();
#endif
        // Launched from hbmenu: keep a log on the SD card instead.
        const std::string log_path = (DATA_DIR / "rs64recomp.log").string();
        // The previous run's log survives one launch: after a crash, relaunching would otherwise erase its trace.
        const std::string previous_path = (DATA_DIR / "rs64recomp.prev.log").string();
        std::remove(previous_path.c_str());
        std::rename(log_path.c_str(), previous_path.c_str());
        // Both streams append: two write-mode streams on one file each keep their own offset and overwrite each other.
        if (FILE* f = std::fopen(log_path.c_str(), "w")) std::fclose(f);
        freopen(log_path.c_str(), "a", stdout);
        freopen(log_path.c_str(), "a", stderr);
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
    }

    // A log line every 3 s for the first 90 s, then every 30 s: runtime progress counters, to tell where a hang sits.
    void start_heartbeat() {
        std::thread([] {
            for (int i = 0;; i++) {
                svcSleepThread(i < 30 ? 3000000000ULL : 30000000000ULL);
                printf("[CVNX] hb %llu ms: gfx %u/%u, rsp %u/%u, vi %u, screen %u, gfx stage %d\n", cvnx_ms_since_boot(),
                       cvnx_hb_gfx_submit.load(), cvnx_hb_gfx_parsed.load(), cvnx_hb_sp_submit.load(), cvnx_hb_sp_done.load(),
                       cvnx_hb_vi.load(), cvnx_hb_screen.load(), cvnx_gfx_stage.load());
            }
        }).detach();
    }

    bool is_rom_extension(std::string ext) {
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        return ext == ".z64" || ext == ".n64" || ext == ".v64";
    }

    // librecomp keeps a validated, byteswapped copy in its config folder. On first boot, import
    // whichever dump the user dropped next to it.
    bool ensure_rom() {
        recomp::check_all_stored_roms();
        if (recomp::is_rom_valid(GAME_ID)) {
            return true;
        }
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(DATA_DIR, ec)) {
            if (!entry.is_regular_file() || !is_rom_extension(entry.path().extension().string())) {
                continue;
            }
            auto result = recomp::select_rom(entry.path(), GAME_ID);
            printf("[CVNX] ROM %s -> validation %d\n", entry.path().c_str(), static_cast<int>(result));
            if (result == recomp::RomValidationError::Good) {
                return true;
            }
        }
        ultramodern::error_handling::message_box(
            "No valid Star Wars: Rogue Squadron ROM found.\n"
            "Copy your USA v1.0 dump (.z64/.n64/.v64) to sdmc:/switch/rs64recomp/");
        return false;
    }

    // Original framerate. In 16:9, Expand + Full HUD, as on PC; GLideN64's adjust mode lays it out like RT64.
    void set_switch_graphics_config() {
        ultramodern::renderer::GraphicsConfig config{};
        config.developer_mode = false;
        config.res_option = ultramodern::renderer::Resolution::Original;
        config.wm_option = ultramodern::renderer::WindowMode::Fullscreen;
        config.hr_option = ultramodern::renderer::HUDRatioMode::Full;
        config.api_option = ultramodern::renderer::GraphicsApi::Auto;
        config.ar_option = nx::widescreen() ? ultramodern::renderer::AspectRatio::Expand
                                            : ultramodern::renderer::AspectRatio::Original;
        config.msaa_option = ultramodern::renderer::Antialiasing::None;
        config.rr_option = ultramodern::renderer::RefreshRate::Original;
        config.hpfb_option = ultramodern::renderer::HighPrecisionFramebuffer::Off;
        config.rr_manual_value = 30;
        config.ds_option = 1;
        ultramodern::renderer::set_graphics_config(config);
    }

    ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) != 0) {
            fprintf(stderr, "[CVNX] SDL_Init failed: %s\n", SDL_GetError());
        }
        return {};
    }

    ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
        step("create window");
        cvnx_window_set_size(nx::screen_width(), nx::screen_height());
        cvnx_window = SDL_CreateWindow(cvnx_game_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         nx::screen_width(), nx::screen_height(), SDL_WINDOW_OPENGL);
        if (cvnx_window == nullptr) {
            fprintf(stderr, "[CVNX] SDL_CreateWindow failed: %s\n", SDL_GetError());
        }
        step("window created");
        return cvnx_window;
    }

    void update_gfx(void*) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                ultramodern::quit();
            }
            cvnx_input_handle_event(event);
        }
        cvnx_perf_update();
    }

    std::string get_game_thread_name(const OSThread* t) {
        switch (t->id) {
            case 1: return "Game Thread IDLE";
            case 3: return "Game Thread MAIN";
            case 4: return "Game Thread EEPROM";
            case 5: return "Game Thread SCHED";
            case 10: return "Game Thread AUDIOMGR";
            case 18: return "Game Thread DMAMGR";
            default: return "Game Thread " + std::to_string(t->id);
        }
    }

    // Once per host VI: the game's own hooks count VIs (g_vi_tick) to pace their waits.
    void vi_callback() {
        g_vi_tick = g_vi_tick + 1;
    }
}

#define REGISTER_FUNC(name) recomp::overlays::register_base_export(#name, name)

// The .nro's path, for Restart Game (nx_menu.cpp).
const char* cvnx_program_path = nullptr;

extern "C" void cvnx_track_thread(const char* name);

int main(int argc, char** argv) {
#ifdef CVNX_REMOTE
    cvnx_trace("main: start");
#endif
    if (argc > 0) cvnx_program_path = argv[0];
    std::set_terminate(log_terminate);
    init_logging();
    cvnx_track_thread("Main");
    printf("[CVNX] Rogue Squadron 64 Recompiled %s for Switch, applet type %d (%llu ms)\n", VERSION_STRING.c_str(),
           static_cast<int>(appletGetAppletType()), cvnx_ms_since_boot());
    nx::load();
    nx::cheats::load();
    cvnx_perf_start();
    start_heartbeat();
#ifdef CVNX_REMOTE
    cvnx_remote_start();
#endif

    recomp::Version project_version{};
    recomp::Version::from_string(VERSION_STRING, project_version);

    step("audio init");
    SDL_InitSubSystem(SDL_INIT_AUDIO);

    step("register game");
    recomp::register_config_path(DATA_DIR);

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
    set_switch_graphics_config();
    step("rom check");

    if (!ensure_rom()) {
        return EXIT_FAILURE;
    }

    // The game starts once the renderer is up (gfx_init_callback), as on PC.
    (void)argc;
    (void)argv;
    step("recomp::start");
    recomp::start(recomp::Configuration{
        .project_version = project_version,
        .window_handle = {},
        .rsp_callbacks = {
            .get_rsp_microcode = rs64::audio::get_rsp_microcode,
        },
        .renderer_callbacks = {
            .create_render_context = cvnx_create_render_context,
        },
        .audio_callbacks = {
            .queue_samples = rs64::audio::queue_samples,
            .get_frames_remaining = rs64::audio::get_frames_remaining,
            .set_frequency = rs64::audio::set_frequency,
        },
        .input_callbacks = {
            .poll_input = recompinput::poll_inputs,
            .get_input = cvnx_get_input,
            .set_rumble = recompinput::set_rumble,
            .get_connected_device_info = cvnx_get_connected_device_info,
        },
        .gfx_callbacks = {
            .create_gfx = create_gfx,
            .create_window = create_window,
            .update_gfx = update_gfx,
        },
        .events_callbacks = {
            .vi_callback = [] {
                vi_callback();
                recompinput::update_rumble();
            },
            .gfx_init_callback = [] {
                if (recomp::is_rom_valid(GAME_ID)) {
                    recomp::start_game(GAME_ID);
                }
            },
        },
        .error_handling_callbacks = {
            .message_box = recompui::message_box,
        },
        .threads_callbacks = {
            .get_game_thread_name = get_game_thread_name,
        },
        .message_queue_control = {},
    });
    step("recomp::start returned");
#ifdef CVNX_REMOTE
    cvnx_remote_stop();
#endif
    // The homebrew loader reuses this process: threads left running would execute unmapped code, and their stacks
    // (like the mods' code memory) would keep pages of the heap the next .nro gets.
    const int threads_left = ultramodern::stop_game_threads();
    printf("[CVNX] game threads stopped, %d left\n", threads_left);
    cvnx_jit_close();
    cvnx_perf_stop();
    step("SDL_Quit");
    SDL_Quit();
    socketExit();
    step("back to the loader");
    // Everything that has to stop is stopped above; the loader reclaims the rest. _exit skips the global C++
    // destructors, which may wait on threads that are gone, and still runs libnx's cleanup before returning to the
    // loader (which then starts the .nro again after Restart Game).
    fflush(nullptr);
#ifdef CVNX_REMOTE
    cvnx_trace("main: _exit");
#endif
    if (threads_left > 0) {
        // Going back to the loader would crash: end the whole process instead (no restart).
        svcExitProcess();
    }
    _exit(EXIT_SUCCESS);
}
