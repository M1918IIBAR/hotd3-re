// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Main Game class — owns all subsystems and runs the main loop.

#pragma once

#include "core/types.h"
#include "core/state_machine.h"
#include "core/config.h"

#include <memory>

struct SDL_Window;

namespace hotd3 {

// Forward declarations
class IRenderer;
class InputManager;
class AudioManager;
class SceneManager;
class Hud;
class VideoPlayer;
class EnemyManager;

class Game {
public:
    Game();
    ~Game();

    // No copy/move
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Initialize all subsystems
    bool init(int argc, char* argv[]);

    // Run the main game loop (blocks until exit)
    void run();

    // Clean shutdown
    void shutdown();

    // Accessors
    bool is_running() const { return running_; }
    StateMachine& state_machine() { return state_machine_; }
    ConfigManager& config_manager() { return config_manager_; }
    IRenderer* renderer() { return renderer_.get(); }
    InputManager* input() { return input_.get(); }
    AudioManager* audio() { return audio_.get(); }
    SceneManager* scene() { return scene_.get(); }
    Hud* hud() { return hud_.get(); }
    VideoPlayer* video() { return video_.get(); }
    EnemyManager* enemies() { return enemies_.get(); }

    // Global instance (set during init, cleared during shutdown)
    static Game* instance();

private:
    // Subsystems
    ConfigManager                config_manager_;
    StateMachine                 state_machine_;
    std::unique_ptr<IRenderer>   renderer_;
    std::unique_ptr<InputManager> input_;
    std::unique_ptr<AudioManager> audio_;
    std::unique_ptr<SceneManager> scene_;
    std::unique_ptr<Hud>          hud_;
    std::unique_ptr<VideoPlayer>  video_;
    std::unique_ptr<EnemyManager> enemies_;

    // SDL
    SDL_Window* window_ = nullptr;

    // State
    bool running_ = false;
    u64  frame_count_ = 0;

    // Timing
    // Original HOTD3 runs at fixed 60fps (16.667ms per frame)
    // We support unlocked framerate with delta time
    f64  target_frame_time_ = 1.0 / 60.0;
    f64  delta_time_ = 0.0;
    f64  total_time_ = 0.0;

    // Title / attract idle timer (seconds without input → attract mode)
    f64  title_idle_timer_ = 0.0;
    static constexpr f64 kTitleIdleTimeout = 15.0; // 15 s, same as arcade

    // Logo display timer
    f64  logo_timer_ = 0.0;
    static constexpr f64 kLogoDisplayTime = 3.0;    // 3 s

    // ── Initialization ───────────────────────────────────────────────────
    bool init_sdl();
    bool init_window();
    bool init_renderer();
    bool init_input();
    bool init_audio();
    bool init_video();
    void init_states();

    // ── Main loop internals ──────────────────────────────────────────────
    void process_events();
    void update(f64 dt);
    void render();

    // ── State handlers ───────────────────────────────────────────────────
    // These implement the behavior for each game state.
    // They're registered with the StateMachine during init_states().

    // Init
    void state_init_enter();
    void state_init_update();

    // Logo
    void state_logo_enter();
    void state_logo_update();
    void state_logo_exit();

    // Title screen
    void state_title_enter();
    void state_title_update();
    void state_title_exit();

    // Attract mode
    void state_attract_enter();
    void state_attract_update();

    // Gameplay
    void state_gameplay_enter();
    void state_gameplay_update();
    void state_gameplay_exit();

    // Boss fight
    void state_boss_enter();
    void state_boss_update();

    static Game* s_instance_;
};

} // namespace hotd3
