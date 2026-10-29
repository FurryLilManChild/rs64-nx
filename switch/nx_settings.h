// Options of the in-game "NX Options" menu (nx_menu.cpp), saved to sdmc:/switch/rs64recomp/nx_options.txt.
// Most apply immediately; the display ones are read once at startup.
#pragma once

#include <initializer_list>

namespace nx {
    enum class Setting {
        ShowFps,
        AspectRatio,
        Resolution,
        TextureLod,
        FrameRate,
        ClockProfile,
        // Controls: per Switch input, the N64 button it presses (nx::N64_CHOICES, input_switch.cpp).
        MapA,
        MapB,
        MapX,
        MapY,
        MapL,
        MapR,
        MapZL,
        MapZR,
        MapPlus,
        MapMinus,
        MapUp,
        MapDown,
        MapLeft,
        MapRight,
        MapLStick,
        MapRStick,
        RightStick,
        PerformanceOverlay,
        MenuTheme,
        MenuOpacity,
        MenuSize,
        MasterVolume,
        TextureFilter,
        InputViewer,
        Count
    };

    struct SettingInfo {
        const char* section;
        const char* key;          // name in nx_options.txt
        const char* label;
        const char* description;
        int default_value;
        // A choice setting stores the index of its choice; a number setting a value in [min, max].
        std::initializer_list<const char*> choices;
        int min = 0;
        int max = 0;
        int step = 0;
        bool needs_restart = false;
        const char* suffix = "";  // after a number value, e.g. "%"
    };

    const SettingInfo& info(Setting setting);
    int get(Setting setting);
    // Clamps to the setting's range.
    void set(Setting setting, int value);
    // Defaults, then the saved file: call once at startup, before the game reads any setting.
    void load();
    void save();
    // Value at startup, for the settings that need a restart: what the renderer is actually using.
    int at_startup(Setting setting);

    inline int resolution_factor() { return at_startup(Setting::Resolution) + 1; }
    // 4x (experimental) draws into a 1920x1080 window instead of 1280x720.
    inline bool output_1080p() { return at_startup(Setting::Resolution) == 3; }
    inline int screen_width() { return output_1080p() ? 1920 : 1280; }
    inline int screen_height() { return output_1080p() ? 1080 : 720; }
    inline bool widescreen() { return at_startup(Setting::AspectRatio) == 0; }
    // Always the game's own 30 fps: the in-between images pair objects by the DK64-style group IDs, which Rogue
    // Squadron's display lists do not carry, so the option is not offered (a saved frame_rate_fps is ignored).
    inline int target_fps() { return 30; }
    // Above 30 fps: in-between images (the game keeps its 30 steps per second), or, in the True mode, frames of the
    // game itself, as fast as the chosen rate.
    inline bool above_30() { return target_fps() > 30; }
    // CV64 always keeps its own 30 steps per second; above 30, in-between images are drawn.
    inline bool true_frames() { return false; }
    inline bool interpolating() { return above_30(); }
}
