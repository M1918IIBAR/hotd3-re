// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Heads-Up Display (HUD) and 2D UI overlay

#pragma once

#include "core/types.h"
#include "render/renderer.h"
#include <filesystem>
#include <string>

namespace hotd3 {

struct PlayerHudState {
    u32 score = 0;
    u32 lives = 3;
    u32 max_lives = 5;
    u32 ammo = 6;
    u32 max_ammo = 6;
    bool show_reload_prompt = false;
    bool is_active = false;
    Vec2 crosshair_pos{ 640.0f, 480.0f };
    Color crosshair_color = Color::red();
};

class Hud {
public:
    Hud();
    ~Hud();

    bool init();
    void shutdown();

    void load_textures(IRenderer* renderer, const std::filesystem::path& extracted_path);

    void update(f64 dt);
    void render(IRenderer* renderer);

    // Player accessors
    PlayerHudState& player1() { return p1_; }
    PlayerHudState& player2() { return p2_; }

    void set_credits(u32 credits) { credits_ = credits; }
    u32 credits() const { return credits_; }

    void show_boss_health(const std::string& name, f32 health_fraction);
    void hide_boss_health();

private:
    bool initialized_ = false;
    PlayerHudState p1_;
    PlayerHudState p2_;
    u32 credits_ = 0;

    // Boss bar
    bool show_boss_bar_ = false;
    std::string boss_name_;
    f32 boss_health_fraction_ = 1.0f;

    // Flashing reload animation
    f32 reload_flash_timer_ = 0.0f;
    bool reload_flash_visible_ = true;

    // Textures
    TextureHandle sight_p1_tex_{};
    TextureHandle sight_p2_tex_{};

    void render_player(IRenderer* renderer, const PlayerHudState& player, bool is_p1);
    void render_crosshair(IRenderer* renderer, const PlayerHudState& player);
    void render_boss_bar(IRenderer* renderer);
};

} // namespace hotd3
