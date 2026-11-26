// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scene and Chapter Manager — manages on-rails stage progression, camera, and actors

#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace hotd3 {

class IRenderer;

enum class ChapterId : u32 {
    Chapter0_Flashback = 0,
    Chapter1_Entrance,
    Chapter2_InfoSystems,
    Chapter3_BioPlant,
    Chapter4_Underground,
    Chapter5_WheelOfFate
};

struct CameraRailWaypoint {
    Vec3 position;
    Vec3 target;
    f32 fov = 60.0f;
    f32 speed = 1.0f;
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    bool init();
    void shutdown();

    void load_chapter(ChapterId chapter);
    void load_stage_geometry(IRenderer* renderer, const std::filesystem::path& extracted_path);
    void update(f64 dt);
    void render(IRenderer* renderer);

    ChapterId current_chapter() const { return current_chapter_; }
    const Vec3& camera_position() const { return current_camera_pos_; }
    const Vec3& camera_target() const { return current_camera_target_; }

    void advance_camera_rail(f32 delta);

private:
    bool initialized_ = false;
    ChapterId current_chapter_ = ChapterId::Chapter0_Flashback;

    Vec3 current_camera_pos_{ 0.0f, 1.5f, 0.0f };
    Vec3 current_camera_target_{ 0.0f, 1.5f, 5.0f };
    f32 rail_progress_ = 0.0f;

    std::vector<CameraRailWaypoint> rail_waypoints_;
    std::vector<MeshHandle> stage_meshes_;
};

} // namespace hotd3
