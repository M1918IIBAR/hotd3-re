// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/state_machine.h"
#include <cstdio>

namespace hotd3 {

const std::string StateMachine::unknown_state_name_ = "UNKNOWN";

StateMachine::StateMachine() = default;
StateMachine::~StateMachine() = default;

void StateMachine::register_state(GameState state, const std::string& name,
                                  StateCallback on_enter,
                                  StateCallback on_update,
                                  StateCallback on_exit,
                                  bool allow_pause) {
    StateInfo info{};
    info.state       = state;
    info.name        = name;
    info.on_enter    = std::move(on_enter);
    info.on_update   = std::move(on_update);
    info.on_exit     = std::move(on_exit);
    info.allow_pause = allow_pause;

    states_[static_cast<u32>(state)] = std::move(info);
}

void StateMachine::change_state(GameState new_state) {
    auto new_it = states_.find(static_cast<u32>(new_state));
    if (new_it == states_.end()) {
        std::fprintf(stderr, "[StateMachine] ERROR: Unknown state %u\n",
                     static_cast<u32>(new_state));
        return;
    }

    // Exit current state
    auto cur_it = states_.find(static_cast<u32>(current_state_));
    if (cur_it != states_.end() && cur_it->second.on_exit) {
        cur_it->second.on_exit();
    }

    previous_state_ = current_state_;
    current_state_  = new_state;

    std::printf("[StateMachine] %s -> %s\n",
                game_state_to_string(previous_state_),
                game_state_to_string(current_state_));

    // Enter new state
    if (new_it->second.on_enter) {
        new_it->second.on_enter();
    }
}

void StateMachine::update() {
    if (is_paused_) return;

    auto it = states_.find(static_cast<u32>(current_state_));
    if (it != states_.end() && it->second.on_update) {
        it->second.on_update();
    }
}

const std::string& StateMachine::current_state_name() const {
    auto it = states_.find(static_cast<u32>(current_state_));
    if (it != states_.end()) {
        return it->second.name;
    }
    return unknown_state_name_;
}

void StateMachine::pause() {
    auto it = states_.find(static_cast<u32>(current_state_));
    if (it != states_.end() && !it->second.allow_pause) return;

    if (!is_paused_) {
        is_paused_ = true;
        pre_pause_state_ = current_state_;
        std::printf("[StateMachine] PAUSED (was: %s)\n",
                    game_state_to_string(current_state_));
    }
}

void StateMachine::unpause() {
    if (is_paused_) {
        is_paused_ = false;
        std::printf("[StateMachine] UNPAUSED (resuming: %s)\n",
                    game_state_to_string(current_state_));
    }
}

const char* game_state_to_string(GameState state) {
    switch (state) {
        case GameState::Init:          return "Init";
        case GameState::Logo:          return "Logo";
        case GameState::AttractMode:   return "AttractMode";
        case GameState::TitleScreen:   return "TitleScreen";
        case GameState::CreditInsert:  return "CreditInsert";
        case GameState::ModeSelect:    return "ModeSelect";
        case GameState::ChapterSelect: return "ChapterSelect";
        case GameState::Loading:       return "Loading";
        case GameState::Gameplay:      return "Gameplay";
        case GameState::BossFight:     return "BossFight";
        case GameState::BossIntro:     return "BossIntro";
        case GameState::ChapterClear:  return "ChapterClear";
        case GameState::PathSelect:    return "PathSelect";
        case GameState::Continue:      return "Continue";
        case GameState::GameOver:      return "GameOver";
        case GameState::Ending:        return "Ending";
        case GameState::Rankings:      return "Rankings";
        case GameState::ServiceMenu:   return "ServiceMenu";
        case GameState::Paused:        return "Paused";
        case GameState::Options:       return "Options";
        case GameState::Cutscene:      return "Cutscene";
        default:                       return "UNKNOWN";
    }
}

} // namespace hotd3
