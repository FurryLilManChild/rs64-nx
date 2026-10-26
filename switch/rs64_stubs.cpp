// Switch definitions for symbols the game-side src/ files expect from the PC frontend (src/main, src/input,
// src/video/rt64_render_context.cpp) and from the RT64 fork's Factor 5 renderer. GLideN64 draws the game here,
// so the renderer hooks are no-ops and the renderer counters stay zero.
#include <atomic>
#include <cstdint>

#include "nx_settings.h"

extern "C" {
    volatile uint8_t* volatile g_recomp_rdram_for_wp_raw = nullptr;

    // RT64 per-frame workload counts, read by osDpGetCounters (src/game/upstream_compat.cpp).
    std::atomic<uint32_t> g_rs64_frame_tris{0};
    std::atomic<uint32_t> g_rs64_frame_draws{0};
    std::atomic<uint32_t> g_rs64_frame_texloads{0};

    // gfx-task trace counters (ultramodern events.cpp, off by default).
    volatile long long g_rs64_pdl_us = 0;
    volatile unsigned g_f5_task_hops = 0;
    volatile unsigned g_f5_task_faces = 0;

    // F5 renderer lighting and tow-cable hooks (rt64_gbi_f3dfactor5.cpp on PC).
    void rs64_f5_map_node(unsigned, unsigned) {}
    void rs64_f5_map_sprite(unsigned, unsigned) {}
    void rs64_f5_cable_submit(uint32_t, uint32_t, uint32_t) {}

    // Horizontal widening of the game's view culls in 16:9: the 4:3 frustum widened to the 16:9 screen.
    float rs64_cull_widen(void) { return nx::widescreen() ? (16.0f / 9.0f) / (4.0f / 3.0f) : 1.0f; }

    // Output pixels per native pixel.
    float rs64_res_scale(void) { return static_cast<float>(nx::resolution_factor()); }

    // Analog throttle (a PC input feature): negative means none, and the game keeps its own speed.
    float rs64_throttle_live(void) { return -1.0f; }
    float rs64_throttle_cruise_live(void) { return -1.0f; }

    // PC window controls and touch layout; the NX Options menu handles quitting on Switch.
    int rs64_get_fullscreen(void) { return 1; }
    void rs64_set_fullscreen(int) {}
    void rs64_toggle_fullscreen(void) {}
    void rs64_touch_layout_request(void) {}
}

// Quit from the game's own menu (PC: closes the window); the Switch build quits through ultramodern.
namespace ultramodern { void quit(); }
extern "C" void rs64_menu_request_quit(void) { ultramodern::quit(); }

bool rs64_renderdoc_active() { return false; }
void rs64_renderdoc_trigger(uint32_t) {}

// Mode-change blanking state shared with RT64's present queue on PC (lib/rt64/src/hle/rt64_rs64_transition.h).
std::atomic<uint32_t> g_rs64_cpu_cleared_fb{0};
std::atomic<uint64_t> g_rs64_cpu_cleared_wid{0};
std::atomic<uint32_t> g_rs64_transition_gate_fb{0};
std::atomic<uint64_t> g_rs64_last_workload_id{0};

// Throttle hook bodies call into src/input/input_host.cpp on PC; without an analog throttle they keep the game's value.
extern "C" float rs64_throttle_hook(uint8_t*, float target, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {
    return target;
}
