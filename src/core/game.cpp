// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/game.h"
#include "render/renderer.h"
#include "input/input_manager.h"
#include "audio/audio_manager.h"
#include "scene/scene_manager.h"
#include "ui/hud.h"
#include "physics/hit_detection.h"
#include "formats/format_registry.h"
#include "video/video_player.h"

#include <SDL.h>
#include <cstdio>
#include <chrono>
#include <filesystem>

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

    // Initialize scene manager
    scene_ = std::make_unique<SceneManager>();
    if (!scene_->init())  return false;

    // Initialize formats / asset registry
    formats::FormatRegistry::instance().init(config_manager_.config().game_data_path);

    // Initialize HUD overlay
    hud_ = std::make_unique<Hud>();
    if (!hud_->init()) return false;
    hud_->load_textures(renderer_.get(), config_manager_.config().game_data_path.parent_path() / "extracted");

    // Initialize video player
    if (!init_video()) return false;

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
    hud_.reset();
    video_.reset();

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
    audio_ = std::make_unique<AudioManager>();
    audio_->set_paths(config_manager_.config().game_data_path,
                      config_manager_.config().game_data_path.parent_path() / "extracted");
    return audio_->init();
}

bool Game::init_video() {
    std::printf("[Game] Initializing video player...\n");
    video_ = std::make_unique<VideoPlayer>();
    return video_->init();
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

        if (hud_) {
            const auto& p1_input = input_->player(0);
            f32 screen_w = static_cast<f32>(config_manager_.config().screen_width);
            f32 screen_h = static_cast<f32>(config_manager_.config().screen_height);

            hud_->player1().crosshair_pos = {
                p1_input.cursor_pos.x * screen_w,
                p1_input.cursor_pos.y * screen_h
            };

            // Player 1 Trigger / Shooting
            if (p1_input.is_pressed(Button::Trigger)) {
                if (hud_->player1().ammo > 0) {
                    hud_->player1().ammo--;
                    if (audio_) {
                        audio_->play_sfx("gun08"); // Shotgun blast sound
                    }
                } else {
                    if (audio_) {
                        audio_->play_sfx("cursor02"); // Empty click
                    }
                }
            }

            // Player 1 Reload
            if (p1_input.is_pressed(Button::Reload) || (!p1_input.gun_on_screen && p1_input.is_pressed(Button::Trigger))) {
                hud_->player1().ammo = hud_->player1().max_ammo;
                if (audio_) {
                    audio_->play_sfx("gun_set"); // Shotgun pump reload sound
                }
            }

            // Coin insert
            if (p1_input.is_pressed(Button::Coin)) {
                hud_->set_credits(hud_->credits() + 1);
                if (audio_) {
                    audio_->play_sfx("cursor01");
                }
            }
        }
    }

    if (audio_) {
        audio_->update(dt);
    }
    if (scene_) {
        scene_->update(dt);
    }
    if (hud_) {
        hud_->update(dt);
    }
    if (video_) {
        video_->update(dt);
    }

    state_machine_.update();
}

void Game::render() {
    if (!renderer_) return;

    renderer_->begin_frame(Color::black());

    // If a video is playing, it takes the whole screen
    if (video_ && video_->is_playing()) {
        video_->render(renderer_.get());
    } else {
        if (scene_) {
            scene_->render(renderer_.get());
        }
        if (hud_) {
            hud_->render(renderer_.get());
        }
    }

    renderer_->end_frame();
}

// ── State handlers ────────────────────────────────────────────────────────────

void Game::state_init_enter() {
    std::printf("[Init] Starting initialization...\n");
}

void Game::state_init_update() {
    // Immediately advance to logo display
    state_machine_.change_state(GameState::Logo);
}

// ── Logo ──────────────────────────────────────────────────────────────────────

void Game::state_logo_enter() {
    std::printf("[Logo] Displaying studio logos...\n");
    logo_timer_ = 0.0;

    // TODO: load Sega / WOW Entertainment logo textures from extracted/SPR/
    // For now the screen stays black and we auto-advance.
}

void Game::state_logo_update() {
    logo_timer_ += delta_time_;
    if (logo_timer_ >= kLogoDisplayTime) {
        state_machine_.change_state(GameState::TitleScreen);
    }
}

void Game::state_logo_exit() {
    // TODO: release logo textures
}

// ── Title Screen ──────────────────────────────────────────────────────────────

void Game::state_title_enter() {
    std::printf("[Title] Press Start!\n");
    title_idle_timer_ = 0.0;

    // Start title BGM (bgm001.adx — theme music plays on loop at title screen)
    if (audio_) {
        audio_->play_bgm("bgm001", true);
    }

    // TODO: load title background texture:
    //   extracted/SPR/<archive>/scr_p_nload_0000.dds (title splash)
    // TODO: render "Press Start" text overlay, flashing at 1Hz
}

void Game::state_title_update() {
    title_idle_timer_ += delta_time_;

    // Any player input resets the idle timer
    if (input_) {
        const auto& p1 = input_->player(0);
        if (p1.is_pressed(Button::Trigger) ||
            p1.is_pressed(Button::Reload)  ||
            p1.is_pressed(Button::Start)   ||
            p1.is_pressed(Button::Coin)) {
            title_idle_timer_ = 0.0;
        }

        // Start button → begin game
        if (p1.is_pressed(Button::Start)) {
            if (hud_ && hud_->credits() > 0) {
                hud_->set_credits(hud_->credits() - 1);
                state_machine_.change_state(GameState::Gameplay);
                return;
            }
        }
    }

    // No input for kTitleIdleTimeout seconds → attract mode
    if (title_idle_timer_ >= kTitleIdleTimeout) {
        state_machine_.change_state(GameState::AttractMode);
    }
}

void Game::state_title_exit() {
    if (audio_) {
        audio_->stop_bgm();
    }
    // TODO: release title textures
}

// ── Attract Mode ─────────────────────────────────────────────────────────────

void Game::state_attract_enter() {
    std::printf("[Attract] Playing attract sequence...\n");

    // Open and play the attract-mode FMV.
    // The original game uses mvi/TITLE_US_CS.SFD (Sofdec, MPEG-1 video + ADX).
    // We open it here; actual decode is pending full Sofdec integration.
    if (video_) {
        namespace fs = std::filesystem;
        fs::path sfd_path =
            config_manager_.config().game_data_path.parent_path()
            / "game_data" / "mvi" / "TITLE_US_CS.SFD";

        if (video_->open_file(sfd_path.string())) {
            video_->play();
            std::printf("[Attract] Playing %s\n", sfd_path.string().c_str());
        } else {
            std::printf("[Attract] SFD not found — returning to title in %gs\n",
                        kTitleIdleTimeout);
        }
    }
}

void Game::state_attract_update() {
    // Any player input → back to title
    if (input_) {
        const auto& p1 = input_->player(0);
        if (p1.is_pressed(Button::Trigger) ||
            p1.is_pressed(Button::Start)   ||
            p1.is_pressed(Button::Coin)) {
            if (video_) video_->stop();
            state_machine_.change_state(GameState::TitleScreen);
            return;
        }
    }

    // Video finished → return to title
    if (!video_ || video_->has_finished() || !video_->is_playing()) {
        state_machine_.change_state(GameState::TitleScreen);
    }
}

// ── Gameplay ─────────────────────────────────────────────────────────────────

void Game::state_gameplay_enter() {
    std::printf("[Gameplay] Starting gameplay — Chapter 1\n");
    if (scene_) {
        scene_->load_chapter(ChapterId::Chapter1_Entrance);
        scene_->load_stage_geometry(renderer_.get(),
            config_manager_.config().game_data_path.parent_path() / "extracted");
    }
    if (audio_) {
        audio_->play_bgm("bgmst1", true);
    }
}

void Game::state_gameplay_update() {
    // Core gameplay loop — to be implemented in Phase 2:
    // 1. Advance camera along rail waypoints
    // 2. Spawn enemies from stage event table (EVTBL.BIN)
    // 3. Update enemy AI (patrol → detect → attack → death)
    // 4. Process hit detection: shotgun ray vs enemy SphereColliders
    // 5. Update score, lives, chapter progress
    // 6. Check chapter end condition → next chapter or boss fight
}

void Game::state_gameplay_exit() {
    if (audio_) {
        audio_->stop_bgm();
    }
    // TODO: unload chapter geometry / enemy objects
}

// ── Boss Fight ────────────────────────────────────────────────────────────────

void Game::state_boss_enter() {
    std::printf("[Boss] Boss encounter!\n");
    if (audio_) {
        audio_->play_bgm("bgmboss", true);
    }
    if (hud_) {
        // Show boss health bar at 100 %
        hud_->set_boss_health(1.0f);
    }
}

void Game::state_boss_update() {
    // Boss AI — to be implemented in Phase 2:
    // 1. Cycle through attack patterns (charge, projectile, grab)
    // 2. Detect hits on weak points (SphereCollider per weak point)
    // 3. Advance damage phases (3 phases typical for HOTD3 bosses)
    // 4. On death: trigger cutscene, restore normal gameplay state
}

} // namespace hotd3
