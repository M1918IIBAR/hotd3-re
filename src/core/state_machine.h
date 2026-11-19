// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Game state machine — mirrors the original HOTD3 game flow.
// States identified via RE of hod3pc.exe and cross-reference with
// Hotd3pc_ArcadeMod's analysis.

#pragma once

#include "core/types.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace hotd3 {

// ── Game States ──────────────────────────────────────────────────────────────
// These mirror the original game's internal state machine.
// The original binary uses an integer state variable at a fixed memory address;
// these enum values should match once the exact mapping is confirmed via RE.
enum class GameState : u32 {
    // Boot / initialization
    Init            = 0,
    
    // Pre-game states
    Logo            = 1,    // Sega logo, WOW Entertainment logo
    AttractMode     = 2,    // Demo playback / attract sequence
    TitleScreen     = 3,    // "Press Start" title
    
    // Credit / coin system (arcade mode)
    CreditInsert    = 4,    // Waiting for credits
    
    // Game setup
    ModeSelect      = 5,    // 1P / 2P selection
    ChapterSelect   = 6,    // Chapter selection (1-4, or EX chapter)
    
    // Core gameplay
    Loading         = 10,   // Loading chapter assets
    Gameplay        = 11,   // Active gameplay (on-rails shooting)
    BossFight       = 12,   // Boss encounter
    BossIntro       = 13,   // Boss name/weakness display
    
    // Between-chapter
    ChapterClear    = 20,   // Chapter results screen
    PathSelect      = 21,   // Branching path choice (some chapters)
    
    // End states
    Continue        = 30,   // Continue countdown
    GameOver        = 31,   // Game over screen
    Ending          = 32,   // Ending cinematic
    Rankings        = 33,   // High score entry / display
    
    // Special
    ServiceMenu     = 90,   // Arcade service/test menu
    Paused          = 91,   // PC pause menu
    Options         = 92,   // PC options screen
    
    // Cutscenes (FMV)
    Cutscene        = 100,  // Playing an FMV
    
    Count
};

// ── State transition callback ────────────────────────────────────────────────
using StateCallback = std::function<void()>;

struct StateInfo {
    GameState   state;
    std::string name;
    StateCallback on_enter;
    StateCallback on_update;
    StateCallback on_exit;
    bool        allow_pause = true;  // Can the player pause in this state?
};

// ── State Machine ────────────────────────────────────────────────────────────
class StateMachine {
public:
    StateMachine();
    ~StateMachine();

    // Register a state with its callbacks
    void register_state(GameState state, const std::string& name,
                       StateCallback on_enter,
                       StateCallback on_update,
                       StateCallback on_exit,
                       bool allow_pause = true);

    // Transition to a new state
    void change_state(GameState new_state);

    // Update the current state (called every frame)
    void update();

    // Query
    GameState current_state() const { return current_state_; }
    GameState previous_state() const { return previous_state_; }
    const std::string& current_state_name() const;

    // Pause support
    void pause();
    void unpause();
    bool is_paused() const { return is_paused_; }

private:
    GameState   current_state_  = GameState::Init;
    GameState   previous_state_ = GameState::Init;
    bool        is_paused_      = false;
    GameState   pre_pause_state_ = GameState::Init;

    std::unordered_map<u32, StateInfo> states_;

    static const std::string unknown_state_name_;
};

// ── Utility ──────────────────────────────────────────────────────────────────
const char* game_state_to_string(GameState state);

} // namespace hotd3
