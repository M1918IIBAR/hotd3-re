// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/game.h"
#include "render/renderer.h"
#include "input/input_manager.h"

#include <SDL.h>
#include <cstdio>
#include <chrono>

namespace hotd3 {

Game* Game::s_instance_ = nullptr;

Game::Game() = default;

Game::~Game() {
    shutdown();
}

Game* Game::instance() {
    return s_instance_;
}

bool Game::init(int argc, char* argv[]) {
    s_instance_ = this;

    std::printf("=== HOTD3-RE v0.1.0 ===\n");
    std::printf("The House of the Dead III: Reverse Engineered\n\n");

    // Load config
    auto config_path = ConfigManager::default_config_path();
    if (!config_manager_.load(config_path)) {
        std::printf("[Game] Using default configuration\n");
    }

    // Parse command line args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--game-data" && i + 1 < argc) {
            config_manager_.config().game_data_path = argv[++i];
        } else if (arg == "--fullscreen") {
            config_manager_.config().fullscreen = true;
        } else if (arg == "--windowed") {
            config_manager_.config().fullscreen = false;
        } else if (arg == "--vulkan") {
            config_manager_.config().renderer_backend = "vulkan";
        } else if (arg == "--dx11") {
            config_manager_.config().renderer_backend = "dx11";
        } else if (arg == "--debug") {
            config_manager_.config().debug_overlay = true;
        }
    }

    // Validate game data path
    if (!config_manager_.config().game_data_path.empty()) {
        if (!config_manager_.validate_game_data(config_manager_.config().game_data_path)) {
            std::fprintf(stderr, "[Game] WARNING: Game data path may be invalid: %s\n",
                        config_manager_.config().game_data_path.string().c_str());
        }
    }

    // Initialize subsystems
    if (!init_sdl())      return false;
    if (!init_window())   return false;
    if (!init_renderer()) return false;
    if (!init_input())    return false;
    if (!init_audio())    return false;

    // Register game states
    init_states();

    running_ = true;
    state_machine_.change_state(GameState::Init);

    std::printf("[Game] Initialization complete\n\n");
    return true;
}

void Game::run() {
    using Clock = std::chrono::high_resolution_clock;

    auto previous_time = Clock::now();

    while (running_) {
        auto current_time = Clock::now();
        delta_time_ = std::chrono::duration<f64>(current_time - previous_time).count();
        previous_time = current_time;

        // Cap delta time to prevent spiral of death
        if (delta_time_ > 0.25) {
            delta_time_ = 0.25;
        }

        total_time_ += delta_time_;
        frame_count_++;

        process_events();
        update(delta_time_);
        render();
    }
}

void Game::shutdown() {
    std::printf("[Game] Shutting down...\n");

    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }

    input_.reset();
    audio_.reset();
    scene_.reset();

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();

    // Save config on exit
    config_manager_.save();

    s_instance_ = nullptr;
    std::printf("[Game] Shutdown complete\n");
}

// ── SDL Init ─────────────────────────────────────────────────────────────────

bool Game::init_sdl() {
    std::printf("[Game] Initializing SDL2...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "[Game] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool Game::init_window() {
    auto& cfg = config_manager_.config();

    std::printf("[Game] Creating window (%ux%u, %s)...\n",
                cfg.screen_width, cfg.screen_height,
                cfg.fullscreen ? "fullscreen" : "windowed");

    u32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    if (cfg.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    // Add Vulkan flag if using Vulkan backend
    if (cfg.renderer_backend == "vulkan") {
        flags |= SDL_WINDOW_VULKAN;
    }

    window_ = SDL_CreateWindow(
        "The House of the Dead III — HOTD3-RE",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(cfg.screen_width),
        static_cast<int>(cfg.screen_height),
        flags
    );

    if (!window_) {
        std::fprintf(stderr, "[Game] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool Game::init_renderer() {
    auto& cfg = config_manager_.config();

    RendererBackend backend = RendererBackend::None;
    if (cfg.renderer_backend == "dx11") {
        backend = RendererBackend::DirectX11;
    } else if (cfg.renderer_backend == "vulkan") {
        backend = RendererBackend::Vulkan;
    }

    std::printf("[Game] Creating renderer (backend: %s)...\n",
                cfg.renderer_backend.c_str());

    renderer_ = create_renderer(backend);
    if (!renderer_) {
        std::fprintf(stderr, "[Game] Failed to create renderer backend '%s'\n",
                    cfg.renderer_backend.c_str());
        return false;
    }

    RenderSettings settings{};
    settings.width        = cfg.screen_width;
    settings.height       = cfg.screen_height;
    settings.fullscreen   = cfg.fullscreen;
    settings.vsync        = cfg.vsync;
    settings.msaa_samples = cfg.msaa_samples;
    settings.max_fps      = cfg.max_fps;

    if (!renderer_->init(window_, settings)) {
        std::fprintf(stderr, "[Game] Renderer initialization failed\n");
        return false;
    }

    std::printf("[Game] Renderer: %s\n", renderer_->backend_name().c_str());
    return true;
}

bool Game::init_input() {
    std::printf("[Game] Initializing input system...\n");
    input_ = std::make_unique<InputManager>();
    return input_->init(window_);
}

bool Game::init_audio() {
    std::printf("[Game] Initializing audio system...\n");
    // TODO: Initialize OpenAL Soft
    return true;
}

void Game::init_states() {
    std::printf("[Game] Registering game states...\n");

    state_machine_.register_state(GameState::Init, "Init",
        [this]() { state_init_enter(); },
        [this]() { state_init_update(); },
        nullptr, false);

    state_machine_.register_state(GameState::Logo, "Logo",
        [this]() { state_logo_enter(); },
        [this]() { state_logo_update(); },
        [this]() { state_logo_exit(); },
        false);

    state_machine_.register_state(GameState::TitleScreen, "TitleScreen",
        [this]() { state_title_enter(); },
        [this]() { state_title_update(); },
        [this]() { state_title_exit(); });

    state_machine_.register_state(GameState::AttractMode, "AttractMode",
        [this]() { state_attract_enter(); },
        [this]() { state_attract_update(); },
        nullptr);

    state_machine_.register_state(GameState::Gameplay, "Gameplay",
        [this]() { state_gameplay_enter(); },
        [this]() { state_gameplay_update(); },
        [this]() { state_gameplay_exit(); });

    state_machine_.register_state(GameState::BossFight, "BossFight",
        [this]() { state_boss_enter(); },
        [this]() { state_boss_update(); },
        nullptr);
}

// ── Main loop ────────────────────────────────────────────────────────────────

void Game::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running_ = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    if (renderer_) {
                        renderer_->resize(
                            static_cast<u32>(event.window.data1),
                            static_cast<u32>(event.window.data2));
                    }
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (state_machine_.is_paused()) {
                        state_machine_.unpause();
                    } else {
                        state_machine_.pause();
                    }
                }
                if (event.key.keysym.sym == SDLK_F11) {
                    // Toggle fullscreen
                    bool is_fs = SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window_, is_fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                break;
        }

        // Forward to input manager
        if (input_) {
            input_->process_event(event);
        }
    }
}

void Game::update(f64 dt) {
    if (input_) {
        input_->update();
    }

    state_machine_.update();
}

void Game::render() {
    if (!renderer_) return;

    renderer_->begin_frame(Color::black());

    // TODO: Render based on current state
    // The state's update callback should queue draw calls

    renderer_->end_frame();
}

// ── State handlers (stubs) ───────────────────────────────────────────────────

void Game::state_init_enter() {
    std::printf("[Init] Starting initialization...\n");
}

void Game::state_init_update() {
    // After init, transition to logo
    state_machine_.change_state(GameState::Logo);
}

void Game::state_logo_enter() {
    std::printf("[Logo] Displaying logos...\n");
    // TODO: Load and display Sega / WOW Entertainment logos
}

void Game::state_logo_update() {
    // TODO: Wait for timer or button press, then go to title
    // For now, skip to title
    state_machine_.change_state(GameState::TitleScreen);
}

void Game::state_logo_exit() {
    // TODO: Clean up logo textures
}

void Game::state_title_enter() {
    std::printf("[Title] Press Start!\n");
    // TODO: Load title screen assets, play title BGM
}

void Game::state_title_update() {
    // TODO: Wait for start button, animate "Press Start"
    // If no input for X seconds, go to attract mode
}

void Game::state_title_exit() {
    // TODO: Clean up title assets
}

void Game::state_attract_enter() {
    std::printf("[Attract] Playing attract sequence...\n");
}

void Game::state_attract_update() {
    // TODO: Play demo sequence, return to title on any input
}

void Game::state_gameplay_enter() {
    std::printf("[Gameplay] Starting gameplay...\n");
}

void Game::state_gameplay_update() {
    // TODO: The core game loop:
    // 1. Update camera along rail path
    // 2. Update enemy AI
    // 3. Process player input (shooting, reloading)
    // 4. Hit detection
    // 5. Update score/lives
}

void Game::state_gameplay_exit() {
    // TODO: Clean up chapter resources
}

void Game::state_boss_enter() {
    std::printf("[Boss] Boss encounter!\n");
}

void Game::state_boss_update() {
    // TODO: Boss AI patterns, weak point system
}

} // namespace hotd3
