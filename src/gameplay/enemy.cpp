// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gameplay/enemy.h"
#include <cmath>
#include <algorithm>

namespace hotd3 {

void Enemy::init(u32 enemy_id, EnemyType enemy_type, const Vec3& spawn_pos) {
    id       = enemy_id;
    type     = enemy_type;
    position = spawn_pos;
    velocity = { 0.0f, 0.0f, 0.0f };
    state    = EnemyAIState::Approach;
    state_timer = 0.0f;
    death_fade_timer = 0.0f;
    active   = true;
    is_boss  = (enemy_type == EnemyType::BossDeath);

    switch (enemy_type) {
        case EnemyType::ZombieNormal:
            max_health = 70.0f;
            move_speed = 1.6f;
            attack_windup_time = 0.9f;
            break;
        case EnemyType::ZombieFat:
            max_health = 160.0f;
            move_speed = 1.1f;
            attack_windup_time = 1.1f;
            break;
        case EnemyType::ZombieSecurity:
            max_health = 110.0f;
            move_speed = 2.0f;
            attack_windup_time = 0.75f;
            break;
        case EnemyType::Bat:
            max_health = 35.0f;
            move_speed = 3.2f;
            attack_windup_time = 0.6f;
            break;
        case EnemyType::BossDeath:
            max_health = 1000.0f;
            move_speed = 2.2f;
            attack_windup_time = 1.2f;
            break;
    }

    health = max_health;
    update_hit_volumes();
}

void Enemy::update_hit_volumes() {
    hit_volumes.clear();
    if (!is_alive()) return;

    f32 scale = (type == EnemyType::ZombieFat || is_boss) ? 1.25f : 1.0f;

    // Head collider
    hit_volumes.push_back(SphereCollider{
        { position.x, position.y + 1.55f * scale, position.z },
        0.24f * scale,
        HitBodyPart::Head,
        id
    });

    // Torso collider
    hit_volumes.push_back(SphereCollider{
        { position.x, position.y + 1.05f * scale, position.z },
        0.38f * scale,
        HitBodyPart::Torso,
        id
    });

    // Left / Right arms
    hit_volumes.push_back(SphereCollider{
        { position.x - 0.42f * scale, position.y + 1.10f * scale, position.z },
        0.20f * scale,
        HitBodyPart::LeftArm,
        id
    });
    hit_volumes.push_back(SphereCollider{
        { position.x + 0.42f * scale, position.y + 1.10f * scale, position.z },
        0.20f * scale,
        HitBodyPart::RightArm,
        id
    });
}

void Enemy::update(f64 dt, const Vec3& player_pos, bool& out_did_attack) {
    out_did_attack = false;
    if (!active) return;

    f32 fdt = static_cast<f32>(dt);
    state_timer += fdt;

    if (state == EnemyAIState::Dead) {
        death_fade_timer += fdt;
        if (death_fade_timer >= 1.5f) {
            active = false;
        }
        return;
    }

    Vec3 to_player{
        player_pos.x - position.x,
        0.0f,
        player_pos.z - position.z
    };
    f32 dist_sq = to_player.x * to_player.x + to_player.z * to_player.z;
    f32 dist    = std::sqrt(dist_sq);

    if (dist > 0.001f) {
        yaw = std::atan2(to_player.x, to_player.z);
    }

    switch (state) {
        case EnemyAIState::Idle:
        case EnemyAIState::Patrol:
            if (state_timer >= 0.5f) {
                state = EnemyAIState::Approach;
                state_timer = 0.0f;
            }
            break;

        case EnemyAIState::Approach:
            if (dist <= attack_range) {
                state = EnemyAIState::Windup;
                state_timer = 0.0f;
                velocity = { 0.0f, 0.0f, 0.0f };
            } else if (dist > 0.001f) {
                f32 inv = 1.0f / dist;
                velocity.x = to_player.x * inv * move_speed;
                velocity.z = to_player.z * inv * move_speed;
                position.x += velocity.x * fdt;
                position.z += velocity.z * fdt;
            }
            break;

        case EnemyAIState::Windup:
            if (state_timer >= attack_windup_time) {
                state = EnemyAIState::Attack;
                state_timer = 0.0f;
                out_did_attack = true;
            }
            break;

        case EnemyAIState::Attack:
            // Brief recovery after swinging before winding up again
            if (state_timer >= 0.8f) {
                state = (dist <= attack_range) ? EnemyAIState::Windup : EnemyAIState::Approach;
                state_timer = 0.0f;
            }
            break;

        case EnemyAIState::Stagger:
            if (state_timer >= 0.35f) {
                state = (dist <= attack_range) ? EnemyAIState::Windup : EnemyAIState::Approach;
                state_timer = 0.0f;
            }
            break;

        case EnemyAIState::Dead:
            break;
    }

    update_hit_volumes();
}

u32 Enemy::apply_hit(const HitResult& hit, f32 base_damage) {
    if (!is_alive()) return 0;

    f32 multiplier = 1.0f;
    u32 hit_score  = 100;

    switch (hit.body_part) {
        case HitBodyPart::Head:
            multiplier = 2.5f;
            hit_score  = 300;
            break;
        case HitBodyPart::Torso:
            multiplier = 1.0f;
            hit_score  = 100;
            break;
        case HitBodyPart::LeftArm:
        case HitBodyPart::RightArm:
            multiplier = 0.65f;
            hit_score  = 75;
            break;
        default:
            multiplier = 0.75f;
            break;
    }

    health = std::max(0.0f, health - base_damage * multiplier);

    if (health <= 0.0f) {
        state = EnemyAIState::Dead;
        state_timer = 0.0f;
        hit_volumes.clear();
        return hit_score + (is_boss ? 5000u : 250u);
    }

    // Stagger interrupts windup! (classic HOTD3 cancel mechanic)
    if (state == EnemyAIState::Windup || state == EnemyAIState::Approach) {
        state = EnemyAIState::Stagger;
        state_timer = 0.0f;
    }

    return hit_score;
}

} // namespace hotd3
