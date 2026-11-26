// hotd3-re — The House of the Dead III: Reverse Engineered
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scene/scene_manager.h"
#include "render/renderer.h"
#include "render/common/vertex.h"
#include "formats/format_registry.h"
#include <cstdio>

namespace hotd3 {

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() {
    shutdown();
}

bool SceneManager::init() {
    std::printf("[Scene] Initializing scene manager...\n");
    initialized_ = true;
    return true;
}

void SceneManager::shutdown() {
    if (!initialized_) return;
    rail_waypoints_.clear();
    std::printf("[Scene] Scene manager shutdown complete\n");
    initialized_ = false;
}

void SceneManager::load_chapter(ChapterId chapter) {
    current_chapter_ = chapter;
    rail_progress_ = 0.0f;

    std::printf("[Scene] Loaded Chapter %u\n", static_cast<u32>(chapter));

    // Initialize default rail test waypoints
    rail_waypoints_.clear();
    rail_waypoints_.push_back({ { 0.0f, 1.6f, 0.0f }, { 0.0f, 1.6f, 10.0f }, 60.0f, 1.0f });
    rail_waypoints_.push_back({ { 0.0f, 1.6f, 15.0f }, { 5.0f, 1.6f, 25.0f }, 60.0f, 1.2f });
    rail_waypoints_.push_back({ { 5.0f, 1.6f, 25.0f }, { 5.0f, 1.6f, 40.0f }, 65.0f, 1.5f });
}

void SceneManager::advance_camera_rail(f32 delta) {
    rail_progress_ += delta;
    // Advance camera position along rail
    current_camera_pos_.z += delta * 2.0f;
    current_camera_target_.z += delta * 2.0f;
}

void SceneManager::update(f64 dt) {
    if (!initialized_) return;
    advance_camera_rail(static_cast<f32>(dt));
}

void SceneManager::load_stage_geometry(IRenderer* renderer, const std::filesystem::path& /*extracted_path*/) {
    if (!renderer) return;

    // Build stage test geometry (corridors and pillars)
    auto floor = MeshBuilder::create_quad(20.0f, 100.0f);
    auto floor_handle = renderer->upload_mesh(floor);
    if (floor_handle.valid()) {
        stage_meshes_.push_back(floor_handle);
    }

    auto cube = MeshBuilder::create_cube(1.0f);
    auto cube_handle = renderer->upload_mesh(cube);
    if (cube_handle.valid()) {
        stage_meshes_.push_back(cube_handle);
    }

    std::printf("[Scene] Uploaded %zu stage meshes to GPU\n", stage_meshes_.size());
}

void SceneManager::render(IRenderer* renderer) {
    if (!renderer || !initialized_) return;

    // Render active stage meshes
    for (auto mesh : stage_meshes_) {
        renderer->draw_mesh(mesh, Mat4::identity());
    }
}

} // namespace hotd3
