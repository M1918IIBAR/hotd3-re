// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/hud.h"
#include <cstdio>
#include <algorithm>

namespace hotd3 {

Hud::Hud() {
    p1_.is_active = true;
    p1_.crosshair_color = Color::red();

    p2_.is_active = false;
    p2_.crosshair_color = Color::blue();
}

Hud::~Hud() {
    shutdown();
}

bool Hud::init() {
    std::printf("[HUD] Initializing HUD overlay...\n");
    initialized_ = true;
    return true;
}

void Hud::shutdown() {
    initialized_ = false;
}

void Hud::load_textures(IRenderer* renderer, const std::filesystem::path& extracted_path) {
    if (!renderer) return;

    auto p1_path = extracted_path / "SPR" / "DAT_007" / "scr_sight_a1_0000.dds";
    if (std::filesystem::exists(p1_path)) {
        sight_p1_tex_ = renderer->load_texture_from_file(p1_path.string());
    }

    auto p2_path = extracted_path / "SPR" / "DAT_007" / "scr_sight_b1_0000.dds";
    if (std::filesystem::exists(p2_path)) {
        sight_p2_tex_ = renderer->load_texture_from_file(p2_path.string());
    }
}

void Hud::update(f64 dt) {
    if (!initialized_) return;

    reload_flash_timer_ += static_cast<f32>(dt);
    if (reload_flash_timer_ >= 0.25f) {
        reload_flash_timer_ = 0.0f;
        reload_flash_visible_ = !reload_flash_visible_;
    }

    p1_.show_reload_prompt = (p1_.ammo == 0);
    p2_.show_reload_prompt = (p2_.ammo == 0 && p2_.is_active);
}

void Hud::show_boss_health(const std::string& name, f32 health_fraction) {
    show_boss_bar_ = true;
    boss_name_ = name;
    boss_health_fraction_ = std::clamp(health_fraction, 0.0f, 1.0f);
}

void Hud::hide_boss_health() {
    show_boss_bar_ = false;
}

void Hud::render(IRenderer* renderer) {
    if (!renderer || !initialized_) return;

    if (p1_.is_active) {
        render_player(renderer, p1_, true);
        render_crosshair(renderer, p1_);
    }

    if (p2_.is_active) {
        render_player(renderer, p2_, false);
        render_crosshair(renderer, p2_);
    }

    if (show_boss_bar_) {
        render_boss_bar(renderer);
    }
}

void Hud::render_player(IRenderer* renderer, const PlayerHudState& player, bool is_p1) {
    f32 base_x = is_p1 ? 40.0f : 1000.0f;
    f32 base_y = 900.0f;

    // Draw life bar / indicators
    for (u32 i = 0; i < player.lives; ++i) {
        RectF life_rect{ base_x + i * 28.0f, base_y, 20.0f, 20.0f };
        renderer->draw_rect(life_rect, Color::green(), true);
    }

    // Draw ammo shotgun shells
    for (u32 i = 0; i < player.ammo; ++i) {
        RectF shell_rect{ base_x + i * 16.0f, base_y + 26.0f, 12.0f, 24.0f };
        renderer->draw_rect(shell_rect, Color::yellow(), true);
    }

    // Flash RELOAD prompt
    if (player.show_reload_prompt && reload_flash_visible_) {
        RectF reload_bg{ base_x, base_y - 40.0f, 140.0f, 30.0f };
        renderer->draw_rect(reload_bg, Color::red(), true);
    }
}

void Hud::render_crosshair(IRenderer* renderer, const PlayerHudState& player) {
    f32 cx = player.crosshair_pos.x;
    f32 cy = player.crosshair_pos.y;

    TextureHandle tex = (&player == &p1_) ? sight_p1_tex_ : sight_p2_tex_;
    if (tex.valid()) {
        Sprite sprite{};
        sprite.texture = tex;
        sprite.size = { 48.0f, 48.0f };
        sprite.origin = { 24.0f, 24.0f };
        sprite.tint = Color::white();
        renderer->draw_sprite(sprite, { cx, cy });
        return;
    }

    // Fallback geometric crosshair
    f32 size = 16.0f;
    f32 thickness = 2.0f;

    // Horizontal line
    renderer->draw_rect({ cx - size, cy - thickness * 0.5f, size * 2.0f, thickness }, player.crosshair_color, true);
    // Vertical line
    renderer->draw_rect({ cx - thickness * 0.5f, cy - size, thickness, size * 2.0f }, player.crosshair_color, true);
}

void Hud::render_boss_bar(IRenderer* renderer) {
    f32 bar_w = 600.0f;
    f32 bar_h = 24.0f;
    f32 bar_x = (1280.0f - bar_w) * 0.5f;
    f32 bar_y = 60.0f;

    // Background border
    renderer->draw_rect({ bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4 }, Color::white(), false);
    renderer->draw_rect({ bar_x, bar_y, bar_w, bar_h }, Color{ 0.2f, 0.2f, 0.2f, 0.8f }, true);

    // Current health fill
    f32 current_w = bar_w * boss_health_fraction_;
    if (current_w > 0.0f) {
        renderer->draw_rect({ bar_x, bar_y, current_w, bar_h }, Color::red(), true);
    }
}

} // namespace hotd3
