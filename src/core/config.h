// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Configuration system. Replaces the original game's Windows Registry usage
// with a portable INI/TOML config file.

#pragma once

#include "core/types.h"
#include <string>
#include <filesystem>

namespace hotd3 {

// ── Original registry keys (for reference / migration) ───────────────────────
// The original hod3pc.exe stores settings in:
//   HKCU\Software\SEGA\THE HOUSE OF THE DEAD3\
//     - ScreenMode       (DWORD: 0=window, 1=fullscreen)
//     - ScreenWidth       (DWORD)
//     - ScreenHeight      (DWORD)
//     - SoundVolume       (DWORD: 0-100)
//     - BloodColor        (DWORD: 0=red, 1=green)
//     - InputDevice       (DWORD: 0=mouse, 1=keyboard)
//     - Language          (DWORD)
//     - Life              (DWORD: number of lives)
//     - Credit            (DWORD: number of credits)
//
// Save data is stored in:
//   %APPDATA%\SEGA\THE HOUSE OF THE DEAD3\

// ── Config structure ─────────────────────────────────────────────────────────
struct GameConfig {
    // Video
    u32         screen_width     = 1280;
    u32         screen_height    = 960;
    bool        fullscreen       = false;
    bool        vsync            = true;
    u32         msaa_samples     = 1;
    u32         max_fps          = 0;
    std::string renderer_backend = "dx11";  // "dx11" or "vulkan"

    // Audio
    u32         master_volume    = 100;  // 0-100
    u32         music_volume     = 100;
    u32         sfx_volume       = 100;
    u32         voice_volume     = 100;

    // Gameplay
    u32         lives            = 3;    // Starting lives (arcade default: 3)
    u32         credits          = 9;    // Available credits (arcade: coin-op)
    u32         blood_color      = 0;    // 0=red, 1=green
    u32         difficulty       = 1;    // 0=easy, 1=normal, 2=hard, 3=very hard
    bool        free_play        = false;
    bool        arcade_mode      = true; // true = arcade-accurate, false = PC mode

    // Input
    f32         mouse_sensitivity = 1.0f;
    bool        raw_input        = true;
    bool        show_crosshair   = true;

    // Paths
    std::filesystem::path game_data_path;  // Where original game files are
    std::filesystem::path save_data_path;  // Where to store saves
    std::filesystem::path config_file_path;

    // Debug
    bool        debug_overlay    = false;
    bool        show_hitboxes    = false;
    bool        show_fps         = true;
};

// ── Config Manager ───────────────────────────────────────────────────────────
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Load config from file (or create default)
    bool load(const std::filesystem::path& config_path);

    // Save current config to file
    bool save() const;
    bool save(const std::filesystem::path& config_path) const;

    // Import settings from original game's registry (Windows only)
    bool import_from_registry();

    // Access
    GameConfig&       config()       { return config_; }
    const GameConfig& config() const { return config_; }

    // Validate game data path (check required files exist)
    bool validate_game_data(const std::filesystem::path& path) const;

    // Get default config path
    static std::filesystem::path default_config_path();
    static std::filesystem::path default_save_path();

private:
    GameConfig config_;

    bool parse_ini(const std::string& content);
    std::string serialize_ini() const;
};

} // namespace hotd3
