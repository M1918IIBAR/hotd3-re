// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input/input_manager.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

namespace hotd3 {

InputManager::InputManager() {
    std::memset(player_states_, 0, sizeof(player_states_));
    std::memset(prev_states_, 0, sizeof(prev_states_));
}

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::init(SDL_Window* window) {
    window_ = window;

    // Enable relative mouse mode for FPS-style input? No — HOTD3 uses
    // absolute cursor position (light-gun style).
    // We keep the system cursor visible and track its absolute position.
    SDL_ShowCursor(SDL_DISABLE);  // Hide system cursor, we render our own crosshair

    std::printf("[Input] Initialized (mouse+keyboard)\n");
    return true;
}

void InputManager::shutdown() {
    devices_.clear();
    window_ = nullptr;
}

void InputManager::process_event(const SDL_Event& event) {
    // For now, Player 1 = mouse+keyboard
    auto& p1 = player_states_[0];

    switch (event.type) {
        case SDL_MOUSEMOTION: {
            // Convert absolute mouse position to normalized 0..1 screen coordinates
            int ww, wh;
            SDL_GetWindowSize(window_, &ww, &wh);
            if (ww > 0 && wh > 0) {
                p1.cursor_pos.x = static_cast<f32>(event.motion.x) / static_cast<f32>(ww);
                p1.cursor_pos.y = static_cast<f32>(event.motion.y) / static_cast<f32>(wh);
                p1.cursor_delta.x = static_cast<f32>(event.motion.xrel) / static_cast<f32>(ww);
                p1.cursor_delta.y = static_cast<f32>(event.motion.yrel) / static_cast<f32>(wh);
            }
            p1.gun_on_screen = true;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                p1.buttons[static_cast<u32>(Button::Trigger)] = true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT ||
                event.button.button == SDL_BUTTON_MIDDLE) {
                p1.buttons[static_cast<u32>(Button::Reload)] = true;
                // Reload = shoot off-screen (arcade behavior)
                p1.gun_on_screen = false;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                p1.buttons[static_cast<u32>(Button::Trigger)] = false;
            }
            if (event.button.button == SDL_BUTTON_RIGHT ||
                event.button.button == SDL_BUTTON_MIDDLE) {
                p1.buttons[static_cast<u32>(Button::Reload)] = false;
                p1.gun_on_screen = true;
            }
            break;

        case SDL_KEYDOWN: {
            auto key = event.key.keysym.sym;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                p1.buttons[static_cast<u32>(Button::Start)] = true;
            if (key == SDLK_c)
                p1.buttons[static_cast<u32>(Button::Coin)] = true;
            if (key == SDLK_F1)
                p1.buttons[static_cast<u32>(Button::Service)] = true;
            if (key == SDLK_F2)
                p1.buttons[static_cast<u32>(Button::Test)] = true;
            if (key == SDLK_UP)
                p1.buttons[static_cast<u32>(Button::Up)] = true;
            if (key == SDLK_DOWN)
                p1.buttons[static_cast<u32>(Button::Down)] = true;
            if (key == SDLK_LEFT)
                p1.buttons[static_cast<u32>(Button::Left)] = true;
            if (key == SDLK_RIGHT)
                p1.buttons[static_cast<u32>(Button::Right)] = true;
            break;
        }

        case SDL_KEYUP: {
            auto key = event.key.keysym.sym;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                p1.buttons[static_cast<u32>(Button::Start)] = false;
            if (key == SDLK_c)
                p1.buttons[static_cast<u32>(Button::Coin)] = false;
            if (key == SDLK_F1)
                p1.buttons[static_cast<u32>(Button::Service)] = false;
            if (key == SDLK_F2)
                p1.buttons[static_cast<u32>(Button::Test)] = false;
            if (key == SDLK_UP)
                p1.buttons[static_cast<u32>(Button::Up)] = false;
            if (key == SDLK_DOWN)
                p1.buttons[static_cast<u32>(Button::Down)] = false;
            if (key == SDLK_LEFT)
                p1.buttons[static_cast<u32>(Button::Left)] = false;
            if (key == SDLK_RIGHT)
                p1.buttons[static_cast<u32>(Button::Right)] = false;
            break;
        }
    }
}

void InputManager::update() {
    for (u32 p = 0; p < MAX_PLAYERS; ++p) {
        auto& cur = player_states_[p];
        auto& prev = prev_states_[p];

        // Compute pressed/released edges
        for (u32 b = 0; b < static_cast<u32>(Button::Count); ++b) {
            cur.buttons_pressed[b]  = cur.buttons[b] && !prev.buttons[b];
            cur.buttons_released[b] = !cur.buttons[b] && prev.buttons[b];
        }

        prev = cur;

        // Reset per-frame deltas
        cur.cursor_delta = {0.0f, 0.0f};
    }

    // Update custom devices
    for (auto& device : devices_) {
        // TODO: Route device output to appropriate player
        if (device->is_connected()) {
            device->update(player_states_[0]);
        }
    }
}

const PlayerInput& InputManager::player(u32 index) const {
    if (index >= MAX_PLAYERS) index = 0;
    return player_states_[index];
}

void InputManager::add_device(u32 player_index, std::unique_ptr<IInputDevice> device) {
    std::printf("[Input] Added device: %s (player %u)\n",
                device->name().c_str(), player_index);
    devices_.push_back(std::move(device));
}

u32 InputManager::device_count() const {
    return static_cast<u32>(devices_.size());
}

void InputManager::set_mouse_sensitivity(f32 sensitivity) {
    mouse_sensitivity_ = sensitivity;
}

void InputManager::set_raw_input(bool enabled) {
    raw_input_ = enabled;
    std::printf("[Input] Raw input: %s\n", enabled ? "enabled" : "disabled");
}

} // namespace hotd3
