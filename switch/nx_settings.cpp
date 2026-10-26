#include "nx_settings.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
#ifndef NX_SETTINGS_PATH
#define NX_SETTINGS_PATH "sdmc:/switch/rs64recomp/nx_options.txt"
#endif
    constexpr const char* SETTINGS_PATH = NX_SETTINGS_PATH;
    constexpr int COUNT = static_cast<int>(nx::Setting::Count);
    constexpr std::initializer_list<const char*> OFF_ON = {"Off", "On"};
    // Order matches N64_BITS in input_switch.cpp.
    constexpr std::initializer_list<const char*> N64_BUTTONS = {"Nothing", "A", "B", "Z", "L", "R", "Start",
        "C-Up", "C-Down", "C-Left", "C-Right", "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right"};
    // Before the 30-60 range, the frame rate was a choice saved as frame_rate: 0 for 30 fps, 1 for 60.
    constexpr const char* LEGACY_FRAME_RATE = "frame_rate";

    // Order follows nx::Setting.
    const nx::SettingInfo SETTINGS[COUNT] = {
        {"Overlays", "show_fps", "Show FPS", "Frames shown per second, in the top left corner.", 0, OFF_ON},
        {"Display", "aspect_ratio", "Aspect Ratio",
         "16:9 fills the screen. 4:3 shows the original framing with black bars on the sides.",
         0, {"16:9", "4:3"}, 0, 0, 0, true},
        {"Display", "resolution_scale", "Resolution",
         "Internal resolution. 1x is the N64's own and the lightest, 2x is sharper, 3x "
         "fills the height of the 720p screen. 4x (experimental) renders 1280x960 into a 1080p output, for a docked "
         "console: heavier on the GPU.",
         2, {"1x (320x240)", "2x (640x480)", "3x (960x720)", "4x (1080p, experimental)"}, 0, 0, 0, true},
        {"Display", "texture_lod", "Texture LOD",
         "GLideN64's level of detail emulation. Off draws distant textures sharp.",
         0, OFF_ON, 0, 0, 0, true},
        {"Performance", "frame_rate_fps", "Frame Rate",
         "Frames shown per second. The game runs at its own 30; above that, an in-between image is drawn between two "
         "of its frames, as on PC: every other one at 45, all at 60. Costs one frame of latency. Experimental.",
         30, {}, 30, 60, 5, false, " fps"},
        {"Performance", "clock_profile", "Clock Profile",
         "Clocks while in the game. Docked GPU (handheld only): the official 768 MHz GPU profile. Horizon OC "
         "(handheld only): the docked clocks set in Horizon OC. CPU Boost (needs Horizon OC): the CPU at 1785 MHz, "
         "the official boost clock, docked and handheld.",
         0, {"Console Default", "Docked GPU in Handheld", "Horizon OC Docked Clocks", "CPU Boost (Horizon OC)"}},
        {"Controls", "map_a", "A Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         1, N64_BUTTONS},
        {"Controls", "map_b", "B Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         2, N64_BUTTONS},
        {"Controls", "map_x", "X Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         0, N64_BUTTONS},
        {"Controls", "map_y", "Y Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         0, N64_BUTTONS},
        {"Controls", "map_l", "L Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         4, N64_BUTTONS},
        {"Controls", "map_r", "R Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         5, N64_BUTTONS},
        {"Controls", "map_zl", "ZL Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         3, N64_BUTTONS},
        {"Controls", "map_zr", "ZR Button",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         5, N64_BUTTONS},
        {"Controls", "map_plus", "Plus Button",
         "The N64 button this button presses in the game. Minus and Plus together still open this menu. The menu itself always uses the Switch buttons.",
         6, N64_BUTTONS},
        {"Controls", "map_minus", "Minus Button",
         "The N64 button this button presses in the game. Minus and Plus together still open this menu. The menu itself always uses the Switch buttons.",
         0, N64_BUTTONS},
        {"Controls", "map_up", "D-Pad Up",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         11, N64_BUTTONS},
        {"Controls", "map_down", "D-Pad Down",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         12, N64_BUTTONS},
        {"Controls", "map_left", "D-Pad Left",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         13, N64_BUTTONS},
        {"Controls", "map_right", "D-Pad Right",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         14, N64_BUTTONS},
        {"Controls", "map_lstick", "Left Stick Click",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         0, N64_BUTTONS},
        {"Controls", "map_rstick", "Right Stick Click",
         "The N64 button this button presses in the game. The menu itself always uses the Switch buttons.",
         0, N64_BUTTONS},
        {"Controls", "right_stick", "Right Stick",
         "What the right stick does in the game: the four C buttons (default), the D-pad, or nothing.",
         0, {"C Buttons", "D-Pad", "Nothing"}},
        {"Overlays", "performance_overlay", "Performance Overlay",
         "Top left: frames shown per second, the time of each pass of a frame and the screen refreshes that showed "
         "nothing new.",
         0, OFF_ON},
        {"Menu", "menu_theme", "Menu Theme", "Colors of this menu.", 1, {"Orange", "Gray"}},
        {"Menu", "menu_opacity", "Menu Opacity", "How much of the game shows through this menu: less is more.",
         95, {}, 50, 100, 5, false, "%"},
        {"Menu", "menu_size", "Menu Size", "Size of the text and the boxes of this menu.", 1, {"Small", "Medium", "Large"}},
        {"Audio", "master_volume", "Master Volume", "Volume of everything the game plays, music and sound effects.",
         100, {}, 0, 100, 5, false, "%"},
        {"Display", "texture_filter", "Texture Filter",
         "How textures are smoothed: Standard bilinear filtering, or the N64's own 3-point filter, sharper and closer "
         "to the console. Every shader is compiled again at the next start.",
         0, {"Standard", "N64 3-Point"}, 0, 0, 0, true},
        {"Overlays", "input_viewer", "Input Viewer",
         "The buttons and sticks being pressed, in the bottom right corner.",
         0, OFF_ON},
    };

    std::atomic<int> values[COUNT];
    int startup_values[COUNT];

    int index(nx::Setting setting) { return static_cast<int>(setting); }

    int clamp(const nx::SettingInfo& info, int value) {
        if (info.choices.size() > 0) {
            return std::clamp(value, 0, static_cast<int>(info.choices.size()) - 1);
        }
        return std::clamp(value, info.min, info.max);
    }
}

const nx::SettingInfo& nx::info(Setting setting) { return SETTINGS[index(setting)]; }

int nx::get(Setting setting) { return values[index(setting)].load(std::memory_order_relaxed); }

void nx::set(Setting setting, int value) {
    values[index(setting)].store(clamp(info(setting), value), std::memory_order_relaxed);
}

// "key = value" lines; unknown keys and malformed lines are skipped, out-of-range values clamped.
void nx::load() {
    for (int i = 0; i < COUNT; i++) values[i] = SETTINGS[i].default_value;
    bool legacy_60 = false;
    bool rate_saved = false;
    if (FILE* file = std::fopen(SETTINGS_PATH, "r")) {
        char line[128];
        while (std::fgets(line, sizeof(line), file) != nullptr) {
            char key[64];
            char text[16];
            if (std::sscanf(line, " %63[a-z_] = %15s", key, text) != 2) continue;
            char* end = nullptr;
            // strtol saturates instead of overflowing like %d.
            const long value = std::strtol(text, &end, 10);
            if (end == text) continue;
            if (std::strcmp(key, LEGACY_FRAME_RATE) == 0) legacy_60 = value == 1;
            for (int i = 0; i < COUNT; i++) {
                if (std::strcmp(key, SETTINGS[i].key) == 0) {
                    set(static_cast<Setting>(i), static_cast<int>(std::clamp(value, -1000L, 1000L)));
                    rate_saved = rate_saved || static_cast<Setting>(i) == Setting::FrameRate;
                }
            }
        }
        std::fclose(file);
    }
    if (legacy_60 && !rate_saved) set(Setting::FrameRate, 60);
    for (int i = 0; i < COUNT; i++) startup_values[i] = values[i];
}

int nx::at_startup(Setting setting) { return startup_values[index(setting)]; }

// Written next to the file, then renamed over it, so a full card or a power cut never leaves it truncated.
void nx::save() {
    const std::string temporary = std::string(SETTINGS_PATH) + ".tmp";
    FILE* file = std::fopen(temporary.c_str(), "w");
    bool written = file != nullptr;
    for (int i = 0; written && i < COUNT; i++) {
        written = std::fprintf(file, "%s = %d\n", SETTINGS[i].key, values[i].load(std::memory_order_relaxed)) > 0;
    }
    if (file != nullptr && std::fclose(file) != 0) written = false;
    // The SD card's rename does not replace an existing file.
    if (!written || (std::remove(SETTINGS_PATH) != 0 && errno != ENOENT) ||
        std::rename(temporary.c_str(), SETTINGS_PATH) != 0) {
        std::fprintf(stderr, "[CVNX] cannot write %s\n", SETTINGS_PATH);
        std::remove(temporary.c_str());
    }
}
