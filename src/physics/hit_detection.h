// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Raycasting, hit detection, and bounding volume intersection

#pragma once

#include "core/types.h"
#include <vector>

namespace hotd3 {

struct Ray {
    Vec3 origin;
    Vec3 direction; // Normalized direction vector
};

enum class HitBodyPart : u32 {
    None = 0,
    Head,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Prop
};

struct HitResult {
    bool hit = false;
    f32 distance = 0.0f;
    Vec3 hit_point;
    Vec3 hit_normal;
    HitBodyPart body_part = HitBodyPart::None;
    u32 target_id = 0;
};

struct SphereCollider {
    Vec3 center;
    f32 radius = 0.5f;
    HitBodyPart body_part = HitBodyPart::Torso;
    u32 target_id = 0;
};

class HitDetection {
public:
    // Create a 3D ray from screen coordinates [0..width, 0..height] given camera matrices
    static Ray screen_to_ray(const Vec2& screen_pos, const Vec2& screen_size,
                             const Mat4& view, const Mat4& projection);

    // Ray vs Sphere intersection test
    static bool intersect_ray_sphere(const Ray& ray, const SphereCollider& sphere, HitResult& out_result);

    // Shotgun pellet cone raycast against active colliders
    static std::vector<HitResult> cast_shotgun_blast(const Ray& center_ray,
                                                      const std::vector<SphereCollider>& targets,
                                                      f32 spread_angle = 3.5f,
                                                      u32 pellet_count = 8);
};

} // namespace hotd3
