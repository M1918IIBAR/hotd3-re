// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "physics/hit_detection.h"
#include <cmath>
#include <algorithm>

namespace hotd3 {

Ray HitDetection::screen_to_ray(const Vec2& screen_pos, const Vec2& screen_size,
                                const Mat4& /*view*/, const Mat4& /*projection*/) {
    Ray ray{};
    // Normalized Device Coordinates [-1, 1]
    f32 ndc_x = (2.0f * screen_pos.x) / screen_size.x - 1.0f;
    f32 ndc_y = 1.0f - (2.0f * screen_pos.y) / screen_size.y;

    ray.origin = { 0.0f, 1.5f, 0.0f };
    ray.direction = { ndc_x * 0.5f, ndc_y * 0.5f, 1.0f };

    // Normalize direction
    f32 len = std::sqrt(ray.direction.x * ray.direction.x +
                        ray.direction.y * ray.direction.y +
                        ray.direction.z * ray.direction.z);
    if (len > 0.0001f) {
        ray.direction.x /= len;
        ray.direction.y /= len;
        ray.direction.z /= len;
    }

    return ray;
}

bool HitDetection::intersect_ray_sphere(const Ray& ray, const SphereCollider& sphere, HitResult& out_result) {
    Vec3 oc = { ray.origin.x - sphere.center.x,
                ray.origin.y - sphere.center.y,
                ray.origin.z - sphere.center.z };

    f32 b = oc.x * ray.direction.x + oc.y * ray.direction.y + oc.z * ray.direction.z;
    f32 c = (oc.x * oc.x + oc.y * oc.y + oc.z * oc.z) - (sphere.radius * sphere.radius);

    f32 discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return false;
    }

    f32 t = -b - std::sqrt(discriminant);
    if (t < 0.0f) {
        t = -b + std::sqrt(discriminant);
        if (t < 0.0f) {
            return false;
        }
    }

    out_result.hit = true;
    out_result.distance = t;
    out_result.hit_point = {
        ray.origin.x + ray.direction.x * t,
        ray.origin.y + ray.direction.y * t,
        ray.origin.z + ray.direction.z * t
    };
    out_result.hit_normal = {
        (out_result.hit_point.x - sphere.center.x) / sphere.radius,
        (out_result.hit_point.y - sphere.center.y) / sphere.radius,
        (out_result.hit_point.z - sphere.center.z) / sphere.radius
    };
    out_result.body_part = sphere.body_part;
    out_result.target_id = sphere.target_id;

    return true;
}

std::vector<HitResult> HitDetection::cast_shotgun_blast(const Ray& center_ray,
                                                        const std::vector<SphereCollider>& targets,
                                                        f32 /*spread_angle*/,
                                                        u32 /*pellet_count*/) {
    std::vector<HitResult> hits;

    for (const auto& sphere : targets) {
        HitResult result{};
        if (intersect_ray_sphere(center_ray, sphere, result)) {
            hits.push_back(result);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const HitResult& a, const HitResult& b) {
        return a.distance < b.distance;
    });

    return hits;
}

} // namespace hotd3
