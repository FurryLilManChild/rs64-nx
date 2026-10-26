#include "nx_dev_tools.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include <switch.h>

#include "nx_settings.h"

extern "C" void cvnx_interp_totals(uint64_t out[4]);  // gliden64_rt64ex.cpp

namespace {
    std::string text(const char* format, ...) {
        char buffer[64];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        return buffer;
    }

    // CPU, GPU and RAM clocks as the console runs them (clkrst), read once a second while the menu is open.
    std::string clocks_text = "-";

    // Interpolation over the last second.
    uint64_t interp_window[4] = {};
    uint64_t interp_window_ms = 0;
    double interp_share = 0.0, interp_cuts = 0.0, interp_unmatched = 0.0;

    const nx::dev::Tool TOOLS[] = {
        {"Console", "Clocks", "CPU, GPU and RAM clocks the console is running at right now.",
         [] { return clocks_text; }, nullptr},
        {"Interpolation", "Frames With an In-Between Image",
         "Above 30 fps, the share of the game's frames that got an in-between image over the last second.",
         [] { return nx::interpolating() ? text("%.0f%%", interp_share) : std::string("30 fps"); }, nullptr},
        {"Interpolation", "Camera Cuts", "Frames over the last second where the camera jumped, shown without an in-between image.",
         [] { return text("%.1f /s", interp_cuts); }, nullptr},
        {"Interpolation", "Unmatched Matrices",
         "Matrices per in-between image that had no counterpart in the previous frame (an object appearing), drawn "
         "where they are instead of halfway.",
         [] { return text("%.1f per frame", interp_unmatched); }, nullptr},
    };
}

int nx::dev::count() { return static_cast<int>(sizeof(TOOLS) / sizeof(TOOLS[0])); }
const nx::dev::Tool& nx::dev::tool(int index) { return TOOLS[index]; }

bool nx::dev::overlay_shown() { return false; }
std::string nx::dev::overlay_text() { return std::string(); }
long nx::dev::play_time_seconds() { return -1; }
bool nx::dev::request_warp(int, int) { return false; }

std::string nx::dev::read_clocks() {
    if (R_FAILED(clkrstInitialize())) return "clkrst unavailable";
    const PcvModuleId modules[] = {PcvModuleId_CpuBus, PcvModuleId_GPU, PcvModuleId_EMC};
    u32 mhz[3] = {};
    for (int i = 0; i < 3; i++) {
        ClkrstSession session;
        u32 hz = 0;
        if (R_SUCCEEDED(clkrstOpenSession(&session, modules[i], 3))) {
            clkrstGetClockRate(&session, &hz);
            clkrstCloseSession(&session);
        }
        mhz[i] = hz / 1000000;
    }
    clkrstExit();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "CPU %u, GPU %u, RAM %u MHz", mhz[0], mhz[1], mhz[2]);
    return buffer;
}

void nx::dev::update(uint64_t now_ms) {
    if (now_ms - interp_window_ms < 1000) return;
    clocks_text = read_clocks();
    uint64_t totals[4];
    cvnx_interp_totals(totals);
    const double frames = static_cast<double>(totals[0] - interp_window[0]);
    const double interpolated = static_cast<double>(totals[1] - interp_window[1]);
    interp_share = frames > 0 ? 100.0 * interpolated / frames : 0.0;
    interp_cuts = static_cast<double>(totals[2] - interp_window[2]) * 1000.0 / static_cast<double>(now_ms - interp_window_ms);
    interp_unmatched = interpolated > 0 ? static_cast<double>(totals[3] - interp_window[3]) / interpolated : 0.0;
    std::copy(totals, totals + 4, interp_window);
    interp_window_ms = now_ms;
}
