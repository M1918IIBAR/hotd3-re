// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Enemy entity definition, AI state machine, and hit-volume layout.

#pragma once

#include "core/types.h"
#include "physics/hit_detection.h"
#include <vector>

namespace hotd3 {

enum class EnemyType : u32 {
    ZombieNormal = 0,
    ZombieFat,
    ZombieSecurity,
    Bat,
    BossDeath
};

enum class EnemyAIState : u32 {
    Idle = 0,
    Patrol,
    Approach,
    Windup,     // Telegraphing melee/projectile attack
    Attack,     // Deals damage to player if not interrupted
    Stagger,    // Flinching from shotgun hit
    Dead
};

struct Enemy {
    u32          id = 0;
    EnemyType    type = EnemyType::ZombieNormal;
    EnemyAIState state = EnemyAIState::Approach;

    Vec3 position{ 0.0f, 0.0f, 10.0f };
    Vec3 velocity{ 0.0f, 0.0f, 0.0f };
    f32  yaw = 0.0f;

    f32 health     = 100.0f;
    f32 max_health = 100.0f;
    f32 move_speed = 1.6f;
    f32 attack_range = 2.2f;
    f32 attack_windup_time = 0.9f;

    f32 state_timer = 0.0f;
    f32 death_fade_timer = 0.0f;
    bool active = true;
    bool is_boss = false;

    std::vector<SphereCollider> hit_volumes;

    // Initialize stats and hit volumes for the given enemy type.
    void init(u32 enemy_id, EnemyType enemy_type, const Vec3& spawn_pos);

    // Advance AI state machine and rebuild world-space hit volumes.
    // Sets out_did_attack = true on the frame an attack lands on the player.
    void update(f64 dt, const Vec3& player_pos, bool& out_did_attack);

    // Apply damage from a shotgun ray hit. Returns score points awarded.
    u32 apply_hit(const HitResult& hit, f32 base_damage = 35.0f);

    // Rebuild sphere colliders around current world position.
    void update_hit_volumes();

    bool is_alive() const { return active && state != EnemyAIState::Dead; }
};

} // namespace hotd3
