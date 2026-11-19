// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/types.h"
#include "input/input_device.h"
#include <memory>
#include <vector>

struct SDL_Window;
union SDL_Event;

namespace hotd3 {

// HOTD3 supports 2 players
static constexpr u32 MAX_PLAYERS = 2;

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool init(SDL_Window* window);
    void shutdown();

    // Process SDL events (called from main event loop)
    void process_event(const SDL_Event& event);

    // Update all devices and player states (called once per frame)
    void update();

    // Get player input state
    const PlayerInput& player(u32 index) const;

    // Device management
    void add_device(u32 player_index, std::unique_ptr<IInputDevice> device);
    u32  device_count() const;

    // Settings
    void set_mouse_sensitivity(f32 sensitivity);
    void set_raw_input(bool enabled);

private:
    SDL_Window* window_ = nullptr;
    PlayerInput player_states_[MAX_PLAYERS];
    PlayerInput prev_states_[MAX_PLAYERS];
    std::vector<std::unique_ptr<IInputDevice>> devices_;
    f32 mouse_sensitivity_ = 1.0f;
    bool raw_input_ = true;
};

} // namespace hotd3
