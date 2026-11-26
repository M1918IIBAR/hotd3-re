// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// EnemyManager — owns active enemies, wave spawning, hit routing, and rendering.

#pragma once

#include "core/types.h"
#include "gameplay/enemy.h"
#include "scene/scene_manager.h"
#include <vector>

namespace hotd3 {

class IRenderer;

class EnemyManager {
public:
    EnemyManager() = default;
    ~EnemyManager() = default;

    bool init(IRenderer* renderer);
    void shutdown(IRenderer* renderer);

    u32  spawn_enemy(EnemyType type, const Vec3& position);
    void spawn_chapter_wave(ChapterId chapter);
    void clear();

    void update(f64 dt, const Vec3& player_pos, u32& out_player_hits);
    void render(IRenderer* renderer);

    // Gather all active enemy hit spheres for shotgun raycasting.
    std::vector<SphereCollider> collect_colliders() const;

    // Route shotgun pellet hits to target enemies. Returns total score gained.
    u32 process_shotgun_hits(const std::vector<HitResult>& hits);

    u32 alive_count() const;
    const std::vector<Enemy>& enemies() const { return enemies_; }

private:
    std::vector<Enemy> enemies_;
    u32 next_id_ = 1;
    MeshHandle enemy_mesh_;
};

} // namespace hotd3
