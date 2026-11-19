// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Input device abstraction — supports mouse, keyboard, gamepads,
// and USB light-guns (Sinden, GUN4IR, AimTrak).
//
// The original HOTD3 PC uses DirectInput 8 for input and translates
// light-gun coordinates into absolute mouse cursor positions.

#pragma once

#include "core/types.h"
#include <string>

namespace hotd3 {

// ── Button state ─────────────────────────────────────────────────────────────
// HOTD3 input buttons — mapped from arcade panel + PC controls
enum class Button : u32 {
    // Shooting
    Trigger,        // Fire / left mouse button
    Reload,         // Reload / shoot off-screen / right mouse button / middle click
    
    // Arcade buttons
    Start,          // Start / Enter
    Coin,           // Insert credit / C key
    Service,        // Service button / F1
    Test,           // Test button / F2
    
    // Navigation
    Up, Down, Left, Right,
    
    // Special
    Pause,          // ESC
    
    Count
};

// ── Player input state ───────────────────────────────────────────────────────
// Per-player input snapshot. HOTD3 supports 2 players.
struct PlayerInput {
    // Cursor / aim position (normalized 0.0 to 1.0, screen space)
    Vec2 cursor_pos    = {0.5f, 0.5f};
    Vec2 cursor_delta  = {0.0f, 0.0f};

    // Button states
    bool buttons[static_cast<u32>(Button::Count)] = {};
    bool buttons_pressed[static_cast<u32>(Button::Count)] = {};   // Just pressed this frame
    bool buttons_released[static_cast<u32>(Button::Count)] = {};  // Just released this frame

    // Convenience
    bool is_down(Button b) const    { return buttons[static_cast<u32>(b)]; }
    bool is_pressed(Button b) const { return buttons_pressed[static_cast<u32>(b)]; }
    bool is_released(Button b) const { return buttons_released[static_cast<u32>(b)]; }

    // Light-gun specific
    bool gun_on_screen = true;  // Is the gun pointed at the screen?
};

// ── Input device type ────────────────────────────────────────────────────────
enum class InputDeviceType : u32 {
    MouseKeyboard,
    Gamepad,
    LightGun,       // USB light-gun (Sinden, GUN4IR, etc.)
    RawInput,       // Windows Raw Input (for multiple mice)
};

// ── Input device interface ───────────────────────────────────────────────────
class IInputDevice {
public:
    virtual ~IInputDevice() = default;
    virtual InputDeviceType type() const = 0;
    virtual std::string name() const = 0;
    virtual void update(PlayerInput& out) = 0;
    virtual bool is_connected() const = 0;
};

} // namespace hotd3
