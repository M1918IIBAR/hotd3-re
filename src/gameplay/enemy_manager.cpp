// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gameplay/enemy_manager.h"
#include "render/renderer.h"
#include "render/common/vertex.h"
#include <algorithm>
#include <cstdio>

namespace hotd3 {

bool EnemyManager::init(IRenderer* renderer) {
    if (renderer) {
        MeshData cube = MeshBuilder::create_cube(1.0f);
        enemy_mesh_ = renderer->upload_mesh(cube);
    }
    std::printf("[Gameplay] EnemyManager initialized\n");
    return true;
}

void EnemyManager::shutdown(IRenderer* renderer) {
    clear();
    if (renderer && enemy_mesh_.valid()) {
        renderer->destroy_mesh(enemy_mesh_);
        enemy_mesh_ = {};
    }
}

u32 EnemyManager::spawn_enemy(EnemyType type, const Vec3& position) {
    Enemy e;
    e.init(next_id_++, type, position);
    enemies_.push_back(std::move(e));
    return enemies_.back().id;
}

void EnemyManager::spawn_chapter_wave(ChapterId chapter) {
    clear();

    // Initial encounter waves placed along the rail forward (+Z) axis
    switch (chapter) {
        case ChapterId::Chapter1_Entrance:
            spawn_enemy(EnemyType::ZombieNormal,   { -1.8f, 0.0f,  9.0f });
            spawn_enemy(EnemyType::ZombieNormal,   {  1.5f, 0.0f, 11.0f });
            spawn_enemy(EnemyType::ZombieSecurity, {  0.0f, 0.0f, 14.0f });
            break;
        case ChapterId::Chapter5_WheelOfFate:
            spawn_enemy(EnemyType::BossDeath,      {  0.0f, 0.0f, 12.0f });
            break;
        default:
            spawn_enemy(EnemyType::ZombieNormal,   { -1.2f, 0.0f, 10.0f });
            spawn_enemy(EnemyType::ZombieFat,      {  1.2f, 0.0f, 12.0f });
            break;
    }

    std::printf("[Gameplay] Spawned %zu enemies for chapter %u\n",
                enemies_.size(), static_cast<u32>(chapter));
}

void EnemyManager::clear() {
    enemies_.clear();
}

void EnemyManager::update(f64 dt, const Vec3& player_pos, u32& out_player_hits) {
    out_player_hits = 0;

    for (auto& enemy : enemies_) {
        bool did_attack = false;
        enemy.update(dt, player_pos, did_attack);
        if (did_attack) {
            out_player_hits++;
        }
    }

    // Remove fully faded dead enemies
    enemies_.erase(
        std::remove_if(enemies_.begin(), enemies_.end(),
                       [](const Enemy& e) { return !e.active; }),
        enemies_.end());
}

void EnemyManager::render(IRenderer* renderer) {
    if (!renderer || !enemy_mesh_.valid()) return;

    for (const auto& enemy : enemies_) {
        if (!enemy.active) continue;

        f32 scale_y = (enemy.state == EnemyAIState::Dead) ? 0.3f : 1.7f;
        f32 scale_xz = (enemy.type == EnemyType::ZombieFat || enemy.is_boss) ? 0.9f : 0.65f;

        Mat4 transform = Mat4::identity();
        transform.m[0][0] = scale_xz;
        transform.m[1][1] = scale_y;
        transform.m[2][2] = scale_xz;
        transform.m[3][0] = enemy.position.x;
        transform.m[3][1] = enemy.position.y + scale_y * 0.5f;
        transform.m[3][2] = enemy.position.z;

        renderer->draw_mesh(enemy_mesh_, transform);
    }
}

std::vector<SphereCollider> EnemyManager::collect_colliders() const {
    std::vector<SphereCollider> colliders;
    for (const auto& enemy : enemies_) {
        if (!enemy.is_alive()) continue;
        colliders.insert(colliders.end(),
                         enemy.hit_volumes.begin(),
                         enemy.hit_volumes.end());
    }
    return colliders;
}

u32 EnemyManager::process_shotgun_hits(const std::vector<HitResult>& hits) {
    u32 total_score = 0;

    for (const auto& hit : hits) {
        if (!hit.hit) continue;
        for (auto& enemy : enemies_) {
            if (enemy.id == hit.target_id && enemy.is_alive()) {
                total_score += enemy.apply_hit(hit);
                break;
            }
        }
    }

    return total_score;
}

u32 EnemyManager::alive_count() const {
    u32 count = 0;
    for (const auto& enemy : enemies_) {
        if (enemy.is_alive()) ++count;
    }
    return count;
}

} // namespace hotd3
