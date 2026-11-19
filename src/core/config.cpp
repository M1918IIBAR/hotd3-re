// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/config.h"
#include <fstream>
#include <sstream>
#include <cstdio>

#ifdef _WIN32
#include <ShlObj.h>
#endif

namespace hotd3 {

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

bool ConfigManager::load(const std::filesystem::path& config_path) {
    config_.config_file_path = config_path;

    if (!std::filesystem::exists(config_path)) {
        std::printf("[Config] No config file found at %s, using defaults\n",
                    config_path.string().c_str());
        return false;
    }

    std::ifstream file(config_path);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    return parse_ini(ss.str());
}

bool ConfigManager::save() const {
    if (config_.config_file_path.empty()) return false;
    return save(config_.config_file_path);
}

bool ConfigManager::save(const std::filesystem::path& config_path) const {
    // Ensure parent directory exists
    auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(config_path);
    if (!file.is_open()) return false;

    file << serialize_ini();
    std::printf("[Config] Saved to %s\n", config_path.string().c_str());
    return true;
}

bool ConfigManager::import_from_registry() {
#ifdef _WIN32
    // TODO: Read from HKCU\Software\SEGA\THE HOUSE OF THE DEAD3\
    // Keys: ScreenMode, ScreenWidth, ScreenHeight, SoundVolume,
    //       BloodColor, InputDevice, Language, Life, Credit
    std::printf("[Config] Registry import not yet implemented\n");
    return false;
#else
    return false;
#endif
}

bool ConfigManager::validate_game_data(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) return false;

    // Check for key files from the original HOTD3 PC installation
    // TODO: Determine exact filenames via RE
    // Known files: hod3pc.exe, various .dat/.bin data files
    // The bundled HOTD2 files would also be present

    return std::filesystem::is_directory(path);
}

std::filesystem::path ConfigManager::default_config_path() {
#ifdef _WIN32
    // Store config next to executable, or in AppData
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::filesystem::path(path) / "hotd3-re" / "config.ini";
    }
#endif
    return std::filesystem::path("config.ini");
}

std::filesystem::path ConfigManager::default_save_path() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::filesystem::path(path) / "hotd3-re" / "saves";
    }
#endif
    return std::filesystem::path("saves");
}

bool ConfigManager::parse_ini(const std::string& content) {
    // Simple INI parser
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#' || line[0] == ';') continue;

        // Section header
        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) {
                current_section = line.substr(1, end - 1);
            }
            continue;
        }

        // Key=Value
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        start = value.find_first_not_of(" \t");
        if (start != std::string::npos) value = value.substr(start);

        // Apply values
        if (current_section == "Video") {
            if (key == "Width")     config_.screen_width = std::stoul(value);
            if (key == "Height")    config_.screen_height = std::stoul(value);
            if (key == "Fullscreen") config_.fullscreen = (value == "true" || value == "1");
            if (key == "VSync")     config_.vsync = (value == "true" || value == "1");
            if (key == "MSAA")      config_.msaa_samples = std::stoul(value);
            if (key == "MaxFPS")    config_.max_fps = std::stoul(value);
            if (key == "Backend")   config_.renderer_backend = value;
        }
        else if (current_section == "Audio") {
            if (key == "MasterVolume") config_.master_volume = std::stoul(value);
            if (key == "MusicVolume")  config_.music_volume = std::stoul(value);
            if (key == "SFXVolume")    config_.sfx_volume = std::stoul(value);
            if (key == "VoiceVolume")  config_.voice_volume = std::stoul(value);
        }
        else if (current_section == "Gameplay") {
            if (key == "Lives")      config_.lives = std::stoul(value);
            if (key == "Credits")    config_.credits = std::stoul(value);
            if (key == "BloodColor") config_.blood_color = std::stoul(value);
            if (key == "Difficulty") config_.difficulty = std::stoul(value);
            if (key == "FreePlay")   config_.free_play = (value == "true" || value == "1");
            if (key == "ArcadeMode") config_.arcade_mode = (value == "true" || value == "1");
        }
        else if (current_section == "Input") {
            if (key == "MouseSensitivity") config_.mouse_sensitivity = std::stof(value);
            if (key == "RawInput")  config_.raw_input = (value == "true" || value == "1");
            if (key == "Crosshair") config_.show_crosshair = (value == "true" || value == "1");
        }
        else if (current_section == "Paths") {
            if (key == "GameData") config_.game_data_path = value;
            if (key == "SaveData") config_.save_data_path = value;
        }
        else if (current_section == "Debug") {
            if (key == "Overlay")   config_.debug_overlay = (value == "true" || value == "1");
            if (key == "Hitboxes")  config_.show_hitboxes = (value == "true" || value == "1");
            if (key == "ShowFPS")   config_.show_fps = (value == "true" || value == "1");
        }
    }

    return true;
}

std::string ConfigManager::serialize_ini() const {
    std::ostringstream ss;

    ss << "# HOTD3-RE Configuration\n";
    ss << "# The House of the Dead III: Reverse Engineered\n\n";

    ss << "[Video]\n";
    ss << "Width = " << config_.screen_width << "\n";
    ss << "Height = " << config_.screen_height << "\n";
    ss << "Fullscreen = " << (config_.fullscreen ? "true" : "false") << "\n";
    ss << "VSync = " << (config_.vsync ? "true" : "false") << "\n";
    ss << "MSAA = " << config_.msaa_samples << "\n";
    ss << "MaxFPS = " << config_.max_fps << "\n";
    ss << "Backend = " << config_.renderer_backend << "\n\n";

    ss << "[Audio]\n";
    ss << "MasterVolume = " << config_.master_volume << "\n";
    ss << "MusicVolume = " << config_.music_volume << "\n";
    ss << "SFXVolume = " << config_.sfx_volume << "\n";
    ss << "VoiceVolume = " << config_.voice_volume << "\n\n";

    ss << "[Gameplay]\n";
    ss << "Lives = " << config_.lives << "\n";
    ss << "Credits = " << config_.credits << "\n";
    ss << "BloodColor = " << config_.blood_color << "\n";
    ss << "Difficulty = " << config_.difficulty << "\n";
    ss << "FreePlay = " << (config_.free_play ? "true" : "false") << "\n";
    ss << "ArcadeMode = " << (config_.arcade_mode ? "true" : "false") << "\n\n";

    ss << "[Input]\n";
    ss << "MouseSensitivity = " << config_.mouse_sensitivity << "\n";
    ss << "RawInput = " << (config_.raw_input ? "true" : "false") << "\n";
    ss << "Crosshair = " << (config_.show_crosshair ? "true" : "false") << "\n\n";

    ss << "[Paths]\n";
    ss << "GameData = " << config_.game_data_path.string() << "\n";
    ss << "SaveData = " << config_.save_data_path.string() << "\n\n";

    ss << "[Debug]\n";
    ss << "Overlay = " << (config_.debug_overlay ? "true" : "false") << "\n";
    ss << "Hitboxes = " << (config_.show_hitboxes ? "true" : "false") << "\n";
    ss << "ShowFPS = " << (config_.show_fps ? "true" : "false") << "\n";

    return ss.str();
}

} // namespace hotd3
